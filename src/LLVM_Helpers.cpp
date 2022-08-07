#include <LLVM_Backend.h>
#include <Type.h>
#include <Memory.h>

typedef IntegerType *(*LLVM_Integer_Types)(LLVMContext &);

static const LLVM_Integer_Types llvm_int_types[] = {NULL, llvm::Type::getInt8Ty, llvm::Type::getInt16Ty,
												llvm::Type::getInt32Ty, llvm::Type::getInt64Ty,
												llvm::Type::getInt8Ty, llvm::Type::getInt16Ty,
												llvm::Type::getInt32Ty, llvm::Type::getInt64Ty};

static const DIType *debug_types[16] = {};

static const char *type_names[] = {"", "i8", "i16",
									"i32", "i64",
									"u8", "u16",
									"u32", "u64",
									"f32", "f64"};

AllocaInst *
allocate_variable(Function *func, u8 *var_name, Type_Info type, Backend_State backend)
{
	if(func)
	{
		IRBuilder<> temp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
		return temp_builder.CreateAlloca(apoc_type_to_llvm(type, backend), 0, (char *)var_name);
	}
	else
	{
		return backend.builder->CreateAlloca(apoc_type_to_llvm(type, backend), 0, (char *)var_name);
	}
}

llvm::Type *
apoc_type_to_llvm(Type_Info type, Backend_State backend)
{
	if (is_untyped(type))
	{
		if(is_integer(type))
		{
			return llvm::Type::getInt64Ty(*backend.context);
		}
		else if(is_float(type))
		{
			return llvm::Type::getDoubleTy(*backend.context);
		}
	}
	if (is_integer(type))
	{
		return llvm_int_types[type.primitive.size](*backend.context);
	}
	else if (is_float(type))
	{
		if(type.primitive.size == real32) return llvm::Type::getFloatTy(*backend.context);
		else if(type.primitive.size == real64) return llvm::Type::getDoubleTy(*backend.context);
		Assert(false);
	}
	else if (type.type == T_BOOLEAN)
	{
		return llvm::Type::getInt1Ty(*backend.context);
	}
	else if (type.type == T_STRING)
	{
		llvm::Type *u8_type = llvm::Type::getInt8Ty(*backend.context);
		return llvm::PointerType::get(u8_type, 0);
	}
	else if (type.type == T_POINTER)
	{
		llvm::Type *base_type = apoc_type_to_llvm(*type.pointer.type, backend);
		return llvm::PointerType::get(base_type, 0);
	}
	else if (type.type == T_STRUCT)
	{
		auto struct_type = shget(backend.struct_types, type.identifier);
		return struct_type;
	}
	else if (type.type == T_VOID)
	{
		return llvm::Type::getVoidTy(*backend.context);
	}
	Assert(false);
	return NULL;
}

DIType *
to_debug_type(Type_Info type, Debug_Info *debug)
{
	if (is_integer(type))
	{
		DIType *result = (DIType *)debug_types[type.primitive.size];
		if (result)
			return result;

		if (is_signed(type))
		{
			result = debug->builder->createBasicType(type_names[type.primitive.size], 8 * type.primitive.size, dwarf::DW_ATE_signed);
			debug_types[type.primitive.size] = result;
			return result;
		}
		else
		{
			result = debug->builder->createBasicType(type_names[type.primitive.size], 8 * (type.primitive.size - 4), dwarf::DW_ATE_unsigned);
			debug_types[type.primitive.size] = result;
			return result;
		}
	}
	else if (is_float(type))
	{
		DIType *result = (DIType *)debug_types[type.primitive.size];
		if (result)
			return result;

		if(type.primitive.size == real32) 
		{
			result = debug->builder->createBasicType(type_names[type.primitive.size], 32, dwarf::DW_ATE_float);
			debug_types[type.primitive.size] = result;
			return result;
		}
		else if(type.primitive.size == real64) 
		{
			result = debug->builder->createBasicType(type_names[type.primitive.size], 64, dwarf::DW_ATE_float);
			debug_types[type.primitive.size] = result;
			return result;
		}
		Assert(false);
	}
	else if (type.type == T_BOOLEAN)
	{
		static DIType *bool_debug_type = NULL;
		if (bool_debug_type)
			return bool_debug_type;

		bool_debug_type = debug->builder->createBasicType("bool", 8, dwarf::DW_ATE_boolean);
		return bool_debug_type;
	}
	else if (type.type == T_STRING)
	{
		static DIType *string_type = NULL;
		if (string_type)
			return string_type;
		
		Type_Info u8_type = {};
		u8_type.type = T_INTEGER;
		u8_type.primitive.size = ubyte1;
		auto u8_debug = to_debug_type(u8_type, debug);

		// @TODO: change for systems different than 64 bits
		string_type = debug->builder->createPointerType(u8_debug, 64);
		return string_type;
	}
	else if (type.type == T_POINTER)
	{
		// @TODO: change for systems different than 64 bits
		return debug->builder->createPointerType(to_debug_type(*type.pointer.type, debug),  64);
	}
	else if (type.type == T_STRUCT)
	{
		Symbol_Info sym = shget(debug->symbol_map, type.identifier);
		return debug->builder->createStructType(sym.scope, sym.name, sym.file, sym.line_number, sym.size_in_bits, sym.allign_in_bits, 
													sym.flags, sym.derived_from, sym.node_array);
	}
	Assert(false);
	return NULL;
}

Instruction::CastOps
get_cast_type(Type_Info to, Type_Info from, b32 *should_cast)
{
	*should_cast = true;
	if(to.identifier && from.identifier)
	{
		if(vstd_strcmp((char *)to.identifier, (char *)from.identifier))
		{
			*should_cast = false;
			return Instruction::CastOps::ZExt;
		}
	}
	if(is_untyped(to))
	{
		if(is_integer(to))
		{
			to.primitive.size = byte8;
		}
		else if(is_float(to))
		{
			to.primitive.size = real64;
		}
	}
	if(is_untyped(from))
	{
		if(is_integer(from))
		{
			from.primitive.size = byte8;
		}
		else if(is_float(from))
		{
			from.primitive.size = real64;
		}
	}

	if(is_float(to))
	{
		if(is_float(from))
		{
			if(from.primitive.size > to.primitive.size)
				return Instruction::CastOps::FPTrunc;
			if(from.primitive.size < to.primitive.size)
				return Instruction::CastOps::FPExt;
			*should_cast = false;
			return Instruction::CastOps::ZExt;
		}
		if(is_integer(from))
		{
			if(is_signed(from))
			{
				return Instruction::CastOps::SIToFP;
			}
			else
			{
				return Instruction::CastOps::UIToFP;
			}
		}
	}
	else if(is_integer(to))
	{
		if(is_signed(to))
		{
			if(is_integer(from))
			{
				if(is_signed(from))
				{
					if(from.primitive.size < to.primitive.size)
					{
						return Instruction::CastOps::ZExt;
					}
					else if(from.primitive.size > to.primitive.size)
					{
						return Instruction::CastOps::Trunc;
					}
					else
					{
						*should_cast = false;
						return Instruction::CastOps::ZExt;
					}
				}
				else
				{
					size_t signed_size = from.primitive.size - 4;
					
					if(signed_size < to.primitive.size)
					{
						return Instruction::CastOps::SExt;
					}
					else if(signed_size > to.primitive.size)
					{
						return Instruction::CastOps::Trunc;
					}
					else
					{
						return Instruction::CastOps::SExt;
					}
				}
			}
			else if(from.type == T_POINTER)
			{
				return Instruction::CastOps::PtrToInt;
			}
			else
			{
				
				Assert(is_float(from));
				return Instruction::CastOps::FPToSI;
			}
		}
		else
		{
			size_t to_size = to.primitive.size - 4;
			if(is_integer(from))
			{
				if(is_signed(from))
				{
					if(from.primitive.size < to_size)
					{
						return Instruction::CastOps::ZExt;
					}
					else if(from.primitive.size > to_size)
					{
						return Instruction::CastOps::Trunc;
					}
					else
					{
						*should_cast = false;
						return Instruction::CastOps::ZExt;
					}
				}
				else
				{
					size_t signed_size = from.primitive.size - 4;
					
					if(signed_size < to_size)
					{
						return Instruction::CastOps::ZExt;
					}
					else if(signed_size > to_size)
					{
						return Instruction::CastOps::Trunc;
					}
					else
					{
						*should_cast = false;
						return Instruction::CastOps::ZExt;
					}
				}
			}
			else
			{
				Assert(is_float(from));
				return Instruction::CastOps::FPToSI;
			}
		}
	}
	else
	{
		Assert(to.type == T_POINTER);
		if(is_integer(from))
			return Instruction::CastOps::IntToPtr;
		
		Assert(false);
		*should_cast = false;
		return Instruction::CastOps::ZExt;
	}
	*should_cast = false;
	return Instruction::CastOps::ZExt;
}