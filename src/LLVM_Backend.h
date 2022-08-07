
#ifndef CODE_GEN_H
#define CODE_GEN_H

#include <Basic.h>
#include <Lexer.h>
#include <Parser.h>


void
llvm_backend_generate(File_Contents *f, Ast_Node *root);

#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <map>

#define DEBUG_INFO(x) if(f->build_commands.debug_info) { x }

using namespace llvm;

struct Struct_Table
{
	u8 *key;
	StructType *value;
};

struct Function_Table
{
	u8 *key;
	Function *value;
};

struct Backend_State
{
	LLVMContext *context;
	IRBuilder<> *builder;
	Module *module;
	std::map<std::string, AllocaInst *> named_values;
	std::map<std::string, GlobalVariable *> named_globals;
	Struct_Table *struct_types;
	Function_Table *func_table;
};

struct Symbol_Info
{
	unsigned int line_number;
	DINode::DIFlags flags;
	u32 allign_in_bits;
	u64 size_in_bits;
	DINodeArray node_array;
	DIType *derived_from;
	DIFile *file;
	DIScope *scope;
	StringRef name;
};

struct Debug_Symbol_Table
{
	u8 *key;
	Symbol_Info value;
};

struct Debug_Info
{
	DIBuilder *builder;
	DICompileUnit *unit;
	Debug_Symbol_Table *symbol_map;
	Stack scope;
};


void
generate_signatures(File_Contents *f);

llvm::Value *
generate_expression(File_Contents *f, Ast_Node *node, Function *func);

void
generate_statement(File_Contents *f, Ast_Node *root);

void
generate_func(File_Contents *f, Ast_Node *node);

void
generate_prototyes(File_Contents *f);

Function *
generate_func_signature(File_Contents *f, Ast_Node *node);

void
generate_assignment(File_Contents *f, Function *func, Ast_Node *node);

llvm::BasicBlock *
generate_block(File_Contents *f, Ast_Node *node, Function *func, BasicBlock *passed_block, const char *block_name, BasicBlock *to_go);

#endif