#include "BdAST.h"
#include "Tokenizer.h"
#include "CompileError.h"
#include <BindingUtil.h>
#include <sstream>
#include "OpEnums.h"
#include <CastAST.h>

using namespace Birdee;


extern Birdee::Tokenizer SwitchTokenizer(Birdee::Tokenizer&& tokzr);
extern std::unique_ptr<ExprAST> ParseExpressionUnknown();
extern int ParseTopLevel();
extern FunctionAST* GetCurrentPreprocessedFunction();
extern std::unique_ptr<Type> ParseTypeName();

static unique_ptr<ExprAST> outexpr = nullptr;
extern BD_CORE_API Tokenizer tokenizer;
struct BirdeePyContext
{
	py::scoped_interpreter guard;
	py::module main_module;
	py::object orig_scope;
	py::object copied_scope;
	BirdeePyContext()
	{
		main_module = py::module::import("__main__");
		auto sysmod = py::module::import("sys");
		auto patharr = py::cast<py::list>(sysmod.attr("path"));
		patharr.append(cu.directory);
		patharr.append(cu.homepath + "pylib");

		if (cu.is_script_mode)
		{
			PyObject* newdict = PyDict_Copy(main_module.attr("__dict__").ptr());
			copied_scope = py::cast<py::object>(newdict);
		}
		py::module::import("birdeec");
		py::exec("from birdeec import *");
		orig_scope = main_module.attr("__dict__");
	}
};
static BirdeePyContext& InitPython()
{
	static BirdeePyContext context;
	return context;
}


BIRDEE_BINDING_API void RunGenerativeScript()
{
	try
	{
		py::eval_file((cu.directory + "/" + cu.filename).c_str(), InitPython().copied_scope);
	}
	catch (py::error_already_set& e)
	{
		std::cerr<<e.what();
	}
	catch (std::runtime_error& e)
	{
		std::cerr << e.what();
	}
}

BIRDEE_BINDING_API void Birdee_ScriptAST_Phase1(ScriptAST* ths)
{
	auto& env=InitPython();
	try
	{
		py::exec(ths->script.c_str(),env.orig_scope);
	}
	catch (py::error_already_set& e)
	{
		throw CompileError(ths->Pos.line, ths->Pos.pos, string("\nScript exception:\n") + e.what());
	}
	ths->expr = std::move(outexpr);
	outexpr = nullptr;
	if (ths->expr)
	{
		ths->expr->Phase1();
		ths->resolved_type = ths->expr->resolved_type;
	}
}

static void CompileExpr(char* cmd) {
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(cmd)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	outexpr = ParseExpressionUnknown();
	SwitchTokenizer(std::move(old_tok));
}


static ResolvedType ResolveType(string str)
{
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(str)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	SourcePos pos = old_tok.GetSourcePos();
	auto type = ParseTypeName();
	SwitchTokenizer(std::move(old_tok));
	return ResolvedType(*type, pos);
}

static int CompileTopLevel(char* src)
{
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(src)), -1);
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	int ret = ParseTopLevel();
	SwitchTokenizer(std::move(old_tok));
	return ret;
}

static py::object GetNumberLiteral(NumberExprAST& ths)
{
	switch (ths.Val.type)
	{
	case tok_byte:
		return py::int_(ths.Val.v_int);
		break;
	case tok_int:
		return py::int_(ths.Val.v_int);
		break;
	case tok_long:
		return py::int_(ths.Val.v_long);
		break;
	case tok_uint:
		return py::int_(ths.Val.v_uint);
		break;
	case tok_ulong:
		return py::int_(ths.Val.v_ulong);
		break;
	case tok_float:
		return py::float_(ths.Val.v_double);
		break;
	case tok_double:
		return py::float_(ths.Val.v_double);
		break;
	}
	abort();
	return py::int_(0);
}

static auto NewNumberExpr(Token tok, py::object& obj) {
	ResolvedType t;
	t.type = tok;
	if (!t.isNumber())
		throw std::invalid_argument("The type is not a number type!");

	NumberLiteral val;
	val.type = tok;
	if (py::isinstance<py::int_>(obj))
	{
		if(tok == tok_float || tok == tok_double)
			val.v_double = (double)obj.cast<uint64_t>();
		else
			val.v_long = obj.cast<uint64_t>();
	}
	else if (py::isinstance<py::float_>(obj))
	{
		if (tok!=tok_float && tok!=tok_double)
			throw std::invalid_argument("bad input type, expecting an float");
		val.v_double = obj.cast<double>();
	}
	else
	{
		throw std::invalid_argument("bad input type, must be either an integer or a float");
	}

	return new UniquePtr<unique_ptr<StatementAST>>(std::make_unique< NumberExprAST>(val));
}


BIRDEE_BINDING_API void Birdee_AnnotationStatementAST_Phase1(AnnotationStatementAST* ths)
{
	ths->impl->Phase1();
	auto& main_module = InitPython().main_module;
	try
	{
		for (auto& func_name : ths->anno)
		{
			main_module.attr(func_name.c_str())(GetRef(ths->impl));
		}
	}
	catch (py::error_already_set& e)
	{
		throw CompileError(ths->Pos.line, ths->Pos.pos, string("\nScript exception:\n") + e.what());
	}
}

BD_CORE_API bool ParseClassBody(ClassAST* cls);
BD_CORE_API void PushClass(ClassAST* cls);
BD_CORE_API void PopClass();
static void CompileClassBody(ClassAST* cls, const char* src)
{
	PushClass(cls);
	Birdee::Tokenizer toknzr(std::make_unique<Birdee::StringStream>(std::string(src)), -1);
	toknzr.GetNextToken();
	auto old_tok = SwitchTokenizer(std::move(toknzr));
	auto field_cnt = cls->fields.size();
	auto func_cnt = cls->funcs.size();
	while (ParseClassBody(cls))
	{
		if (cls->funcs.size() != func_cnt)
		{
			cls->funcs.back().decl->Phase1();
			func_cnt = cls->funcs.size();
		}
		if (cls->fields.size() != field_cnt)
		{
			cls->fields.back().decl->Phase1();
			field_cnt = cls->fields.size();
		}
	}
	if (tokenizer.CurTok != tok_eof)
		throw std::invalid_argument("Bad token for class body, expecting EOF\n");
	SwitchTokenizer(std::move(old_tok));
	PopClass();
}

namespace Birdee
{
	BD_CORE_API extern string GetTokenString(Token tok);
}

template<Token t1,Token t2>
void RegisterNumCastClass(py::module& m)
{
	string name = string("CastNumberExpr_") + GetTokenString(t1) + "_" + GetTokenString(t2);
	using T = Birdee::CastNumberExpr<t1, t2>;
	py::class_<T, ExprAST>(m, name.c_str())
		.def_static("new", [](UniquePtrStatementAST& expr) { return new UniquePtrStatementAST(std::make_unique<T>(expr.move_expr(), tokenizer.GetSourcePos())); })
		.def_property("expr", [](T& ths) {return GetRef(ths.expr); },
			[](T& ths, UniquePtrStatementAST& v) {
			ths.expr = v.move_expr();
		})
		.def("run", [](T& ths, py::object& func) {func(GetRef(ths.expr)); });
}

//extern template class py::enum_<Token>;
//extern template class py::class_<SourcePos>;
//extern template class py::class_<ResolvedType>;
//extern template class py::class_<StatementAST>;
//extern template class py::class_<ExprAST, StatementAST>;
//extern template class py::class_<ResolvedIdentifierExprAST, ExprAST>;

PYBIND11_MAKE_OPAQUE(std::vector<FieldDef>);
PYBIND11_MAKE_OPAQUE(std::vector<MemberFunctionDef>);

extern void RegisiterClassForBinding2(py::module& m);

void RegisiterClassForBinding(py::module& m)
{
	if (cu.is_script_mode || !cu.is_compiler_mode)
	{
		m.def("clear_compile_unit", []() {cu.Clear(); });
		m.def("top_level", CompileTopLevel);
		m.def("process_top_level", []() {cu.Phase0(); cu.Phase1(); });
		cu.InitForGenerate();
	}
	m.def("class_body", CompileClassBody);
	m.def("resolve_type", ResolveType);
	py::class_ < CompileError>(m, "CompileError")
		.def_readwrite("linenumber", &CompileError::linenumber)
		.def_readwrite("pos", &CompileError::pos)
		.def_readwrite("msg", &CompileError::msg);
	py::class_ < TokenizerError>(m, "TokenizerError")
		.def_readwrite("linenumber", &TokenizerError::linenumber)
		.def_readwrite("pos", &TokenizerError::pos)
		.def_readwrite("msg", &TokenizerError::msg);

	static py::exception<int> CompileErrorExc(m, "CompileException");
	static py::exception<int> TokenizerErrorExc(m, "TokenizerException");
	py::register_exception_translator([](std::exception_ptr p) {
		try {
			if (p) std::rethrow_exception(p);
		}
		catch (const CompileError &e) {
			CompileErrorExc(e.msg.c_str());
		}
		catch (const TokenizerError &e) {
			TokenizerErrorExc(e.msg.c_str());
		}
	});

	m.def("expr", CompileExpr);
	m.def("get_cur_func", GetCurrentPreprocessedFunction);
	m.def("get_func", [](const std::string& name) {
		auto itr = cu.funcmap.find(name);
		if (itr == cu.funcmap.end())
			return GetRef((FunctionAST*)nullptr);
		else
			return itr->second;
	});
	m.def("get_top_level", []() {return GetRef(cu.toplevel); });
	m.def("get_compile_error", []() {return GetRef(CompileError::last_error); });
	m.def("get_tokenizer_error", []() {return GetRef(TokenizerError::last_error); });

	RegisiterClassForBinding2(m);

	RegisiterObjectVector<FieldDef>(m, "FieldDefList");
	RegisiterObjectVector<MemberFunctionDef>(m, "MemberFunctionDefList");

	py::class_<NumberExprAST, ResolvedIdentifierExprAST>(m, "NumberExprAST")
		.def_static("new", NewNumberExpr)
		.def("__str__", [](NumberExprAST& ths) {
			std::stringstream buf;
			ths.ToString(buf);
			return buf.str();
		})
		.def_property("value", GetNumberLiteral, [](NumberExprAST& ths, py::object& obj) {
			if (py::isinstance<py::int_>(obj))
			{
				ths.Val.v_long = obj.cast<uint64_t>();
			}
			else if (py::isinstance<py::float_>(obj))
			{
				ths.Val.v_double = obj.cast<double>();
			}
			else
			{
				throw std::invalid_argument("bad input type, must be either an integer or a float");
			}
		})
		.def_property("type", [](NumberExprAST& ths) {return ths.Val.type; }, [](NumberExprAST& ths, Token tok) {ths.Val.type = tok; })
		.def("run", [](NumberExprAST& ths, py::object& func) {});


	py::enum_ < AccessModifier>(m, "AccessModifier")
		.value("PUBLIC", AccessModifier::access_public)
		.value("PRIVATE", AccessModifier::access_private);

	py::class_ < FieldDef>(m, "FieldDef")
		.def_static("new", [](int index, AccessModifier access, UniquePtrStatementAST& v) {
			return new UniquePtr< FieldDef>(FieldDef(access, move_cast_or_throw< VariableSingleDefAST>(v.ptr), index));
		})
		.def_readwrite("index", &FieldDef::index)
		.def_readwrite("access", &FieldDef::access)
		.def_property("decl", [](FieldDef& ths) {return GetRef(ths.decl); },
			[](FieldDef& ths, UniquePtrStatementAST& v) {ths.decl = move_cast_or_throw< VariableSingleDefAST>(v.ptr); });

	py::class_ < MemberFunctionDef>(m, "MemberFunctionDef")
		.def_static("new", [](AccessModifier access, UniquePtrStatementAST& v) {
			return new UniquePtr< MemberFunctionDef>(MemberFunctionDef(access, move_cast_or_throw< FunctionAST>(v.ptr)));
		})
		.def_readwrite("access", &MemberFunctionDef::access)
		.def_property("decl", [](MemberFunctionDef& ths) {return GetRef(ths.decl); },
			[](MemberFunctionDef& ths, UniquePtrStatementAST& v) {ths.decl = move_cast_or_throw< FunctionAST>(v.ptr); });

	py::class_ < NewExprAST, ExprAST>(m, "NewExprAST")
		.def_property_readonly("args", [](NewExprAST& ths) {return GetRef(ths.args); })
		.def_readwrite("func", &NewExprAST::func)
		.def("run", [](NewExprAST& ths, py::object& func) {
			for (auto &v : ths.args)
				func(GetRef(v));
		});

	py::class_ < ClassAST, StatementAST>(m, "ClassAST")
		.def_readwrite("name", &ClassAST::name)
		.def_property_readonly("fields", [](ClassAST& ths) {return GetRef(ths.fields); })
		.def_property_readonly("funcs", [](ClassAST& ths) {return GetRef(ths.funcs); })
		.def_property_readonly("template_instance_args", [](ClassAST& ths) {return GetRef(ths.template_instance_args); })
		.def_property_readonly("template_source_class", [](ClassAST& ths) {return GetRef(ths.template_source_class); })
		.def_property_readonly("template_param", [](ClassAST& ths) {return GetRef(*(TemplateParameterFake<ClassAST>*)ths.template_param.get()); })
		.def("is_template_instance", &ClassAST::isTemplateInstance)
		.def("is_template", &ClassAST::isTemplate)
		.def("get_unique_name", &ClassAST::GetUniqueName)
		.def("run", [](LocalVarExprAST& ths, py::object& func) {});//fix-me: what to run on ClassAST?
//	unordered_map<reference_wrapper<const string>, int> fieldmap;
//	unordered_map<reference_wrapper<const string>, int> funcmap;
//	int package_name_idx = -1;


	auto member_cls = py::class_ < MemberExprAST, ResolvedIdentifierExprAST>(m, "MemberExprAST");
	py::enum_ < MemberExprAST::MemberType>(member_cls, "AccessModifier")
		.value("ERROR", MemberExprAST::MemberType::member_error)
		.value("PACKAGE", MemberExprAST::MemberType::member_package)
		.value("FIELD", MemberExprAST::MemberType::member_field)
		.value("FUNCTION", MemberExprAST::MemberType::member_function)
		.value("IMPORTED_DIM", MemberExprAST::MemberType::member_imported_dim)
		.value("IMPORTED_FUNCTION", MemberExprAST::MemberType::member_imported_function);

	member_cls
		.def_property("func", [](MemberExprAST& ths) {
			return ths.kind == MemberExprAST::MemberType::member_function ? GetRef(ths.func) : GetNullRef<MemberFunctionDef>();
		}, [](MemberExprAST& ths,  MemberFunctionDef* v) {
			ths.kind = MemberExprAST::MemberType::member_function;
			ths.func = v;
		})
		.def_property("field", [](MemberExprAST& ths) {
			return ths.kind == MemberExprAST::MemberType::member_field ? GetRef(ths.field) : GetNullRef<FieldDef>(); 
		}, [](MemberExprAST& ths, FieldDef* v) {
			ths.kind = MemberExprAST::MemberType::member_field;
			ths.field = v;
		})
		.def_property("imported_func", [](MemberExprAST& ths) {
			return ths.kind == MemberExprAST::MemberType::member_imported_function ? GetRef(ths.import_func) : GetNullRef<FunctionAST>();
		}, [](MemberExprAST& ths, FunctionAST* v) {
			ths.kind = MemberExprAST::MemberType::member_imported_function;
			ths.import_func = v;
		})
		.def_property("imported_dim", [](MemberExprAST& ths) {
			return ths.kind == MemberExprAST::MemberType::member_imported_dim ? GetRef(ths.import_dim) : GetNullRef<VariableSingleDefAST>(); 
		}, [](MemberExprAST& ths, VariableSingleDefAST* v) {
			ths.kind = MemberExprAST::MemberType::member_imported_dim;
			ths.import_dim = v;
		})
		.def("to_string_array",&MemberExprAST::ToStringArray)
		.def_readwrite("kind",&MemberExprAST::kind)
		.def_property("obj", [](MemberExprAST& ths) {return GetRef(ths.Obj); }, [](MemberExprAST& ths, UniquePtrStatementAST& v) {
			ths.Obj=v.move_expr();
		})
		.def("run", [](MemberExprAST& ths, py::object& func) {func(GetRef(ths.Obj)); });

	py::class_ < ScriptAST, ExprAST>(m, "ScriptAST")
		.def_static("new", [](const string& str) { return new UniquePtrStatementAST(std::make_unique<ScriptAST>(str)); })
		.def_property("expr", [](ScriptAST& ths) {return GetRef(ths.expr); }, [](ScriptAST& ths, UniquePtrStatementAST& v) {
			ths.expr = v.move_expr();
		})
		.def_readwrite("script", &ScriptAST::script)
		.def("run", [](ScriptAST& ths, py::object& func) {func(GetRef(ths.expr)); });

	RegisterNumCastClass<tok_int, tok_float>(m);
	RegisterNumCastClass<tok_long, tok_float>(m);
	RegisterNumCastClass<tok_byte, tok_float>(m);
	RegisterNumCastClass<tok_int, tok_double>(m);
	RegisterNumCastClass<tok_long, tok_double>(m);
	RegisterNumCastClass<tok_byte, tok_double>(m);

	RegisterNumCastClass<tok_uint, tok_float>(m);
	RegisterNumCastClass<tok_ulong, tok_float>(m);
	RegisterNumCastClass<tok_uint, tok_double>(m);
	RegisterNumCastClass<tok_ulong, tok_double>(m);

	RegisterNumCastClass<tok_double, tok_int>(m);
	RegisterNumCastClass<tok_double, tok_long>(m);
	RegisterNumCastClass<tok_double, tok_byte>(m);
	RegisterNumCastClass<tok_float, tok_int>(m);
	RegisterNumCastClass<tok_float, tok_long>(m);
	RegisterNumCastClass<tok_float, tok_byte>(m);

	RegisterNumCastClass<tok_double, tok_uint>(m);
	RegisterNumCastClass<tok_double, tok_ulong>(m);
	RegisterNumCastClass<tok_float, tok_uint>(m);
	RegisterNumCastClass<tok_float, tok_ulong>(m);

	RegisterNumCastClass<tok_float, tok_double>(m);
	RegisterNumCastClass<tok_double, tok_float>(m);

	RegisterNumCastClass<tok_int, tok_uint>(m);
	RegisterNumCastClass<tok_long, tok_ulong>(m);

	RegisterNumCastClass<tok_uint, tok_int>(m);
	RegisterNumCastClass<tok_ulong, tok_long>(m);

	/*RegisterNumCastClass<tok_ulong, tok_ulong);
	RegisterNumCastClass<tok_long, tok_long);
	RegisterNumCastClass<tok_byte, tok_byte);
	RegisterNumCastClass<tok_uint, tok_uint);
	RegisterNumCastClass<tok_int, tok_int);
	RegisterNumCastClass<tok_float, tok_float);
	RegisterNumCastClass<tok_double, tok_double);*/

	RegisterNumCastClass<tok_int, tok_long>(m);
	RegisterNumCastClass<tok_int, tok_byte>(m);
	RegisterNumCastClass<tok_int, tok_ulong>(m);
	RegisterNumCastClass<tok_uint, tok_long>(m);
	RegisterNumCastClass<tok_uint, tok_byte>(m);
	RegisterNumCastClass<tok_uint, tok_ulong>(m);
	RegisterNumCastClass<tok_byte, tok_long>(m);
	RegisterNumCastClass<tok_byte, tok_ulong>(m);
	RegisterNumCastClass<tok_long, tok_byte>(m);
	RegisterNumCastClass<tok_ulong, tok_byte>(m);

	RegisterNumCastClass<tok_byte, tok_int>(m);
	RegisterNumCastClass<tok_byte, tok_uint>(m);
	RegisterNumCastClass<tok_long, tok_int>(m);
	RegisterNumCastClass<tok_long, tok_uint>(m);
	RegisterNumCastClass<tok_ulong, tok_int>(m);
	RegisterNumCastClass<tok_ulong, tok_uint>(m);
	
}

extern "C" PyObject * pybind11_init_impl_birdeec() {
	auto m = pybind11::module("birdeec");
	try {
		RegisiterClassForBinding(m);
		return m.ptr();
	}
	catch (pybind11::error_already_set &e) {
		PyErr_SetString(PyExc_ImportError, e.what());
		return nullptr;
	}
	catch (const std::exception &e) {

		PyErr_SetString(PyExc_ImportError, e.what());
		return nullptr;
	}
}
struct myembedded_module
{
	myembedded_module()
	{
		if (cu.is_compiler_mode)
		{
			py::detail::embedded_module mod("birdeec", pybind11_init_impl_birdeec);
		}
	}
}module_initializer;



PYBIND11_MODULE(birdeec, m)
{
	RegisiterClassForBinding(m);
}
