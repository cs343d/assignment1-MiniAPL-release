#include "MiniAPLJIT.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/SimplifyLibCalls.h"
#include "llvm/Transforms/Scalar/GVN.h"


#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <regex>
#include <vector>
#include <cassert>

using namespace llvm;
using namespace llvm::orc;
using namespace std;

class ASTNode;

// -------------------------------------------------
// Miscellaneous helper functions 
// -------------------------------------------------

std::unique_ptr<ASTNode> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static inline
string str(const int i) {
  return to_string(i);
}

bool is_int( const std::string& str ) { // check with regex (does not accept leading zeroes before first digit)
  static constexpr auto max_digits = std::numeric_limits<int>::digits10 ;
  static const std::string ub = std::to_string(max_digits-1) ;
  static const std::regex int_re( "^\\s*([+-]?[1-9]\\d{0," + ub + "}|0)\\s*$" ) ;

  return std::regex_match( str, int_re ) ;
}

// -------------------------------------------------
// Type information for MiniAPL programs
// -------------------------------------------------

enum ExprType {
  EXPR_TYPE_SCALAR,
  EXPR_TYPE_FUNCALL,
  EXPR_TYPE_VARIABLE 
};

class MiniAPLArrayType {
  public:

    vector<int> dimensions;

    int Cardinality() {
      int C = 1;
      for (auto D : dimensions) {
        C *= D;
      }
      return C;
    }

    int length(const int dim) {
      return dimensions.at(dim);
    }

    int dimension() {
      return dimensions.size();
    }
};

std::ostream& operator<<(std::ostream& out, MiniAPLArrayType& tp) {
  out << "[";
  int i = 0;
  for (auto T : tp.dimensions) {
    out << T;
    if (i < (int) (tp.dimensions.size() - 1)) {
      out << ", ";
    }
    i++;
  }
  out << "]";
  return out;
}

// -------------------------------------------------
// AST classes 
// -------------------------------------------------

// The base class for all expression nodes.
class ASTNode {
  public:
    virtual ~ASTNode() = default;

    virtual Value *codegen(Function* F) = 0;
    virtual ExprType GetType() = 0;
    virtual void Print(std::ostream& out) {

    }
};

std::ostream& operator<<(std::ostream& out, ASTNode& tp) {
  tp.Print(out);
  return out;
}

class StmtAST: public ASTNode {
  public:
    virtual bool IsAssign() = 0;
};

class ProgramAST : public ASTNode {
  public:
    std::vector<unique_ptr<StmtAST> > Stmts;
    Value *codegen(Function* F) override;
    virtual ExprType GetType() override { return EXPR_TYPE_FUNCALL; }
};

class ExprStmtAST : public StmtAST {
  public:
    std::unique_ptr<ASTNode> Val;

    bool IsAssign() override { return false; }
    ExprStmtAST(std::unique_ptr<ASTNode> Val_) : Val(std::move(Val_)) {}
    Value *codegen(Function* F) override;
    virtual ExprType GetType() override { return EXPR_TYPE_FUNCALL; }

    virtual void Print(std::ostream& out) override {
      Val->Print(out);
    }
};


class VariableASTNode : public ASTNode {

  public:
    std::string Name;
    VariableASTNode(const std::string &Name) : Name(Name) {}

    Value *codegen(Function* F) override;

    virtual ExprType GetType() override { return EXPR_TYPE_VARIABLE; }

    virtual void Print(std::ostream& out) override {
      out << Name;
    }
};

class AssignStmtAST : public StmtAST {
  public:
    std::unique_ptr<VariableASTNode> Name;
    std::unique_ptr<ASTNode> RHS;

    bool IsAssign() override { return true; }
    Value *codegen(Function* F) override;

    std::string GetName() const { return Name->Name; }

    AssignStmtAST(const std::string& Name_, std::unique_ptr<ASTNode> val_) : Name(new VariableASTNode(Name_)), RHS(std::move(val_)) {}
    virtual ExprType GetType() override { return EXPR_TYPE_FUNCALL; }
    virtual void Print(std::ostream& out) override {
      out << "assign ";
      Name->Print(out);
      out << " = ";
      RHS->Print(out);
    }
};

class NumberASTNode : public ASTNode {
  public:
    int Val;
    NumberASTNode(int Val) : Val(Val) {}

    Value *codegen(Function* F) override;

    virtual ExprType GetType() override { return EXPR_TYPE_SCALAR; }

    virtual void Print(std::ostream& out) override {
      out << Val;
    }
};

class CallASTNode : public ASTNode {

  public:
    std::string Callee;
    std::vector<std::unique_ptr<ASTNode>> Args;
    CallASTNode(const std::string &Callee,
        std::vector<std::unique_ptr<ASTNode>> Args)
      : Callee(Callee), Args(std::move(Args)) {}

    Value *codegen(Function* F) override;
    virtual ExprType GetType() override { return EXPR_TYPE_FUNCALL; }
    virtual void Print(std::ostream& out) override {
      out << Callee << "(";
      for (int i = 0; i < (int) Args.size(); i++) {
        Args.at(i)->Print(out);
        if (i < (int) Args.size() - 1) {
          out << ", ";
        }
      }
      out << ")";
    }
};


// ---------------------------------------------------------------------------
// Some global variables used in parsing, type-checking, and code generation.
// ---------------------------------------------------------------------------
static map<ASTNode*, MiniAPLArrayType> TypeTable;
static map<string, Value*> ValueTable;
static LLVMContext TheContext;
// NOTE: You will probably want to use the Builder in the "codegen" methods
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;
static std::unique_ptr<MiniAPLJIT> TheJIT;

// ---------------------------------------------------------------------------
// LLVM codegen helpers
// ---------------------------------------------------------------------------
IntegerType* intTy(const int width) {
  return IntegerType::get(TheContext, 32);
}

ConstantInt* intConst(const int width, const int i) {
  ConstantInt* const_int32 = ConstantInt::get(TheContext , APInt(width, StringRef(str(i)), 10));
  return const_int32;
}

static void InitializeModuleAndPassManager() {
  // Open a new module.
  TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

  // Create a new pass manager attached to it.
  TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());

  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());

  TheFPM->doInitialization();
}

// NOTE: This utility function generates LLVM IR to print out the string "to_print"
void kprintf_str(Module *mod, BasicBlock *bb, const std::string& to_print) {
  Function *func_printf = mod->getFunction("printf");
  if (!func_printf) {
    PointerType::get(IntegerType::get(mod->getContext(), 8), 0);
    FunctionType *FuncTy9 = FunctionType::get(IntegerType::get(mod->getContext(), 32), true);

    func_printf = Function::Create(FuncTy9, GlobalValue::ExternalLinkage, "printf", mod);
    func_printf->setCallingConv(CallingConv::C);
  }

  IRBuilder <> builder(TheContext);
  builder.SetInsertPoint(bb);


  Value *str = builder.CreateGlobalStringPtr(to_print);

  std::vector <Value *> int32_call_params;
  int32_call_params.push_back(str);

  CallInst::Create(func_printf, int32_call_params, "call", bb);

}

// NOTE: This utility function generates code that prints out the 32 bit input "val" when
// executed.
void kprintf_val(Module *mod, BasicBlock *bb, Value* val) {
  Function *func_printf = mod->getFunction("printf");
  if (!func_printf) {
    PointerType::get(IntegerType::get(mod->getContext(), 8), 0);
    FunctionType *FuncTy9 = FunctionType::get(IntegerType::get(mod->getContext(), 32), true);

    func_printf = Function::Create(FuncTy9, GlobalValue::ExternalLinkage, "printf", mod);
    func_printf->setCallingConv(CallingConv::C);
  }

  IRBuilder <> builder(TheContext);
  builder.SetInsertPoint(bb);


  Value *str = builder.CreateGlobalStringPtr("%d");

  std::vector <Value *> int32_call_params;
  int32_call_params.push_back(str);
  int32_call_params.push_back(val);

  CallInst::Create(func_printf, int32_call_params, "call", bb);
}

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

// ---------------------------------------------------------------------------
// Code generation functions that you should fill in for this assignment
// ---------------------------------------------------------------------------
Value *ProgramAST::codegen(Function* F) {
  // STUDENTS: FILL IN THIS FUNCTION
  return nullptr;
}

Value *AssignStmtAST::codegen(Function* F) {
  // STUDENTS: FILL IN THIS FUNCTION
  return nullptr;
}

Value *ExprStmtAST::codegen(Function* F) {
  // STUDENTS: FILL IN THIS FUNCTION
  return nullptr;
}

Value *NumberASTNode::codegen(Function* F) {
  // STUDENTS: FILL IN THIS FUNCTION
  return nullptr;
}

Value *VariableASTNode::codegen(Function* F) {
  // STUDENTS: FILL IN THIS FUNCTION
  return nullptr;
}

Value *CallASTNode::codegen(Function* F) {
  // STUDENTS: FILL IN THIS FUNCTION
  return nullptr;
}

// ---------------------------------------------------------------------------
// Parser utilities 
// ---------------------------------------------------------------------------
class ParseState {
  public:
    int Position;
    vector<string> Tokens;

    ParseState(vector<string>& Tokens_) : Position(0), Tokens(Tokens_) {}

    bool AtEnd() {
      return Position == (int) Tokens.size();
    }


    string peek() {
      if (AtEnd()) {
        return "";
      }
      return Tokens.at(Position);
    }

    string peek(const int Offset) {
      assert(Position + Offset < Tokens.size());
      return Tokens.at(Position + Offset);
    }

    string eat() {
      auto Current = peek();
      Position++;
      return Current;
    }
};

std::ostream& operator<<(std::ostream& out, ParseState& PS) {
  int i = 0;
  for (auto T : PS.Tokens) {
    if (i == PS.Position) {
      out << " | ";
    }
    out << T << " ";
    i++;
  }
  return out;
}

#define EAT(PS, t) if (PS.eat() != (t)) { return LogError("EAT ERROR"); }

unique_ptr<ASTNode> ParseExpr(ParseState& PS) {
  string Name = PS.eat();
  if (is_int(Name)) {
    return unique_ptr<ASTNode>(new NumberASTNode(stoi(Name)));
  }

  if (PS.peek() == "(") {
    // Parse a function call

    PS.eat(); // consume "("

    vector<unique_ptr<ASTNode> > Args;
    while (PS.peek() != ")") {
      Args.push_back(ParseExpr(PS));
      if (PS.peek() != ")") {
        EAT(PS, ",");
      }
    }
    EAT(PS, ")");

    return unique_ptr<ASTNode>(new CallASTNode(Name, move(Args)));
  } else {
    return unique_ptr<ASTNode>(new VariableASTNode(Name));
  }
}

// ---------------------------------------------------------------------------
// Driver function for type-checking 
// ---------------------------------------------------------------------------
void SetType(map<ASTNode*, MiniAPLArrayType>& Types, ASTNode* Expr) {
  if (Expr->GetType() == EXPR_TYPE_FUNCALL) {
    CallASTNode* Call = static_cast<CallASTNode*>(Expr);
    for (auto& A : Call->Args) {
      SetType(Types, A.get());
    }

    if (Call->Callee == "mkArray") {
      int NDims = static_cast<NumberASTNode*>(Call->Args.at(0).get())->Val;
      vector<int> Dims;
      for (int i = 0; i < NDims; i++) {
        Dims.push_back(static_cast<NumberASTNode*>(Call->Args.at(i + 1).get())->Val);
      }
      Types[Expr] = {Dims};
    } else if (Call->Callee == "reduce") {
      Types[Expr] = Types[Call->Args.back().get()];
      Types[Expr].dimensions.pop_back();
    } else if (Call->Callee == "add" || Call->Callee == "sub") {
      Types[Expr] = Types[Call->Args.at(0).get()];
    } else {
      Types[Expr] = Types[Call->Args.at(0).get()];
    }
  } else if (Expr->GetType() == EXPR_TYPE_SCALAR) {
    Types[Expr] = {{1}};
  } else if (Expr->GetType() == EXPR_TYPE_VARIABLE) {
    string ExprName = static_cast<VariableASTNode*>(Expr)->Name;
    for (auto T : Types) {
      auto V = T.first;
      if (V->GetType() == EXPR_TYPE_VARIABLE) {
        string Name = static_cast<VariableASTNode*>(V)->Name;
        if (Name == ExprName) {
          Types[Expr] = T.second;
        }
      }
    }
  }

}

int main(const int argc, const char** argv) {
  assert(argc == 2);

  // Read in the source code file to a string
  string target_file = argv[1];

  std::ifstream t(target_file);
  std::string str((std::istreambuf_iterator<char>(t)),
      std::istreambuf_iterator<char>());

  // Tokenize the file
  vector<string> Tokens;
  string NextToken = "";
  for (int i = 0; i < (int) str.size(); i++) {
    char NC = str[i];
    if (NC == ',' || NC == '(' || NC == ')' || NC == ';' || NC == '=') {
      if (NextToken != "") {
        Tokens.push_back(NextToken);
      }
      NextToken = string("") + NC;
      Tokens.push_back(NextToken);
      NextToken = "";
    } else if (!isspace(NC)) {
      NextToken += NC;
    } else {
      assert(isspace(NC));
      if (NextToken != "") {
        Tokens.push_back(NextToken);
      }
      NextToken = "";
    }
  }
  if (NextToken != "") {
    Tokens.push_back(NextToken);
  }

  vector<vector<string> > Stmts;
  vector<string> Toks;
  for (auto t : Tokens) {
    if (t == ";") {
      Stmts.push_back(Toks);
      Toks = {};
    } else {
      Toks.push_back(t);
    }
  }

  if (Toks.size() > 0) {
    Stmts.push_back(Toks);
  }

  // Parse each statement
  vector<unique_ptr<StmtAST> > ParsedStmts;
  for (auto S : Stmts) {
    ParseState PS(S);
    assert(S.size() > 0);
    if (PS.peek() != "assign") {
      unique_ptr<ASTNode> value = ParseExpr(PS);
      ParsedStmts.push_back(std::unique_ptr<StmtAST>(new ExprStmtAST(move(value))));
    } else {
      PS.eat(); // eat "assign"

      string Var = PS.eat();

      if (PS.eat() != "=") {
      } else {
        unique_ptr<ASTNode> value = ParseExpr(PS);
        ParsedStmts.push_back(std::unique_ptr<StmtAST>(new AssignStmtAST(Var, move(value))));
      }
    }
  }

  // Collect the statements into a program
  ProgramAST prog;
  prog.Stmts = move(ParsedStmts);

  // Infer types
  for (auto& S : prog.Stmts) {
    StmtAST* SA = S.get();
    if (SA->IsAssign()) {
      AssignStmtAST* Assign = static_cast<AssignStmtAST*>(SA);
      SetType(TypeTable, Assign->RHS.get());
      TypeTable[Assign->Name.get()] = TypeTable[Assign->RHS.get()];
    } else {
      ExprStmtAST* Expr = static_cast<ExprStmtAST*>(SA);
      SetType(TypeTable, Expr->Val.get());
    }
  }

  TheModule = llvm::make_unique<Module>("MiniAPL Module " + target_file, TheContext);
  std::vector<Type *> Args(0, Type::getDoubleTy(TheContext));
  FunctionType *FT =
    FunctionType::get(Type::getVoidTy(TheContext), Args, false);

  Function *F =
    Function::Create(FT, Function::ExternalLinkage, "__anon_expr", TheModule.get());
  BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(&(F->getEntryBlock()));

  prog.codegen(F);

  Builder.CreateRet(nullptr);

  // NOTE: You may want to uncomment this line to see the LLVM IR you have generated
  // TheModule->print(errs(), nullptr);

  // Initialize the JIT, compile the module to a function,
  // find the function and then run it.
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  TheJIT = llvm::make_unique<MiniAPLJIT>();
  InitializeModuleAndPassManager();
  auto H = TheJIT->addModule(std::move(TheModule));


  auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
  void (*FP)() = (void (*)())(intptr_t)cantFail(ExprSymbol.getAddress());
  assert(FP != nullptr);
  FP();

  TheJIT->removeModule(H);

  return 0;
}
