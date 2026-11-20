#include "js_emit.hpp"
namespace js {
void emitLiteral(ES& es, Expr& e, const TypeInfo& expect) {
    switch (e.lt) {
        case TT::Str:
            es.w("\"" + escapeCsStr(e.val) + "\"");
            break;
        case TT::TBlk: {
            es.w("\"" + escapeCsStr(e.val) + "\"");
            break;
        }
        case TT::Chr:
            es.w("'" + escapeCsChar(e.val) + "'");
            break;
        case TT::Null:
            es.w("null");
            break;
        case TT::Bool:
            es.w(e.val);
            break;
        case TT::Int:
        case TT::Flt:
            es.w(e.val);
            break;
        default:
            es.w(e.val);
            break;
    }
    (void)expect;
}

void emitIdent(ES& es, Expr& e) {
    std::string name = e.name;
    if (name == "super") {
        es.w("base");
        return;
    }
    if (name == "this") {
        es.w("this");
        return;
    }
    if (name == "System.out") {
        es.w("Console.Out");
        return;
    }
    if (name == "System.err") {
        es.w("Console.Error");
        return;
    }
    if (name == "System.in") {
        es.w("Console.In");
        return;
    }
    if (es.ctx.staticImports.count(name)) {
        es.w(es.ctx.staticImports[name]);
        return;
    }
    if (es.ctx.needStaticMath) {
        if (name == "PI") { es.w("Math.PI"); return; }
        if (name == "E") { es.w("Math.E"); return; }
        std::string mn = staticMathName(name);
        if (mn != name) {
            es.w("Math." + mn);
            return;
        }
    }
    std::string seg = firstSeg(name);
    if (seg == name) {
        std::string mapped = mapJavaTypeFqn(name, true);
        if (!mapped.empty() && name != "var") {
            es.w(mapped);
            return;
        }
    }
    es.w(csIdent(name));
}

void emitFieldExpr(ES& es, Expr& e) {
    if (e.name == "this" && e.obj) {
        es.w("this");
        return;
    }
    if (!e.obj) {
        es.w(e.name);
        return;
    }
    std::string path = pathText(e);
    if (path == "System.out") {
        es.w("Console.Out");
        return;
    }
    if (path == "System.err") {
        es.w("Console.Error");
        return;
    }
    if (path == "System.in") {
        es.w("Console.In");
        return;
    }
    TypeInfo ot = inferExprType(es, *e.obj);
    std::string recv = captureExprText(es, *e.obj, P_POST);
    if (ot.known) {
        auto rcit = es.ctx.recordComps.find(ot.name);
        if (rcit != es.ctx.recordComps.end()) {
            for (auto& cn : rcit->second) {
                if (cn == e.name) {
                    es.w(recv + "." + pascalCase(e.name));
                    return;
                }
            }
        }
        if (ot.dims > 0 && e.name == "length") {
            es.w(recv + ".Length");
            return;
        }
        if (ot.name == "string" && e.name == "length") {
            es.w(recv + ".Length");
            return;
        }
        if (ot.name == "Math" && e.name == "PI") {
            es.w("Math.PI");
            return;
        }
        if (ot.name == "Math" && e.name == "E") {
            es.w("Math.E");
            return;
        }
        static const std::map<std::string, std::string> wrapperFields = {
            {"MAX_VALUE", "MaxValue"}, {"MIN_VALUE", "MinValue"},
            {"POSITIVE_INFINITY", "PositiveInfinity"}, {"NEGATIVE_INFINITY", "NegativeInfinity"},
            {"NaN", "NaN"}, {"SIZE", "Size"}, {"BYTES", "Bytes"}, {"TYPE", "Type"}};
        if (isNumericCs(stripNullable(ot.name)) || ot.name == "bool") {
            auto wf = wrapperFields.find(e.name);
            if (wf != wrapperFields.end()) {
                if (wf->second == "Size" || wf->second == "Bytes") {
                    static const std::map<std::string, std::string> bits = {
                        {"byte", "8"}, {"sbyte", "8"}, {"short", "16"}, {"ushort", "16"},
                        {"int", "32"}, {"uint", "32"}, {"long", "64"}, {"ulong", "64"},
                        {"char", "16"}, {"float", "32"}, {"double", "64"}};
                    auto bt = bits.find(stripNullable(ot.name));
                    if (bt != bits.end()) {
                        es.w(bt->second);
                        return;
                    }
                }
                if (wf->second == "Type") {
                    es.w("typeof(" + stripNullable(ot.name) + ")");
                    return;
                }
                if (wf->second != "Size" && wf->second != "Bytes" && wf->second != "Type") {
                    es.w(stripNullable(ot.name) + "." + wf->second);
                    return;
                }
            }
        }
        if (ot.name == "BigInteger") {
            static const std::map<std::string, std::string> biConst = {
                {"ONE", "BigInteger.One"}, {"ZERO", "BigInteger.Zero"},
                {"TEN", "new BigInteger(10)"}, {"TWO", "new BigInteger(2)"},
                {"MINUS_ONE", "BigInteger.MinusOne"}};
            auto bc = biConst.find(e.name);
            if (bc != biConst.end()) {
                es.ctx.needNumerics = true;
                es.w(bc->second);
                return;
            }
        }
        if (es.ctx.enumClassTypes.count(ot.name)) {
            if (e.name == "Ordinal") {
                es.w(recv + ".Ordinal");
                return;
            }
            if (e.name == "Name") {
                es.w(recv + ".Name");
                return;
            }
        }
        if (es.ctx.simpleEnums.count(ot.name)) {
            if (e.name == "ordinal") {
                es.w("(int)" + recv);
                return;
            }
        }
    }
    if (ot.known && ot.name == "System" ) {
        static const std::map<std::string, std::string> sysFields = {
            {"in", "Console.In"}, {"out", "Console.Out"}, {"err", "Console.Error"}};
        auto sf = sysFields.find(e.name);
        if (sf != sysFields.end()) {
            es.w(sf->second);
            return;
        }
    }
    if (e.obj->k == EK::Ident) {
        std::string base = e.obj->name;
        std::string mappedBase = mapJavaTypeFqn(base, true);
        if (mappedBase.empty()) mappedBase = base;
        if (mappedBase == "Integer" || mappedBase == "int") {
            if (e.name == "MAX_VALUE") { es.w("int.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("int.MinValue"); return; }
        }
        if (mappedBase == "Long" || mappedBase == "long") {
            if (e.name == "MAX_VALUE") { es.w("long.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("long.MinValue"); return; }
        }
        if (mappedBase == "Double" || mappedBase == "double") {
            if (e.name == "MAX_VALUE") { es.w("double.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("double.MinValue"); return; }
            if (e.name == "NaN") { es.w("double.NaN"); return; }
            if (e.name == "POSITIVE_INFINITY") { es.w("double.PositiveInfinity"); return; }
            if (e.name == "NEGATIVE_INFINITY") { es.w("double.NegativeInfinity"); return; }
        }
        if (mappedBase == "Float" || mappedBase == "float") {
            if (e.name == "MAX_VALUE") { es.w("float.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("float.MinValue"); return; }
        }
        if (mappedBase == "Character" || mappedBase == "char") {
            if (e.name == "MAX_VALUE") { es.w("char.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("char.MinValue"); return; }
        }
        if (mappedBase == "StandardCharsets") {
            if (e.name == "UTF_8") { es.ctx.needText = true; es.w("Encoding.UTF8"); return; }
            if (e.name == "UTF_16") { es.ctx.needText = true; es.w("Encoding.Unicode"); return; }
            if (e.name == "UTF_16BE") { es.ctx.needText = true; es.w("Encoding.BigEndianUnicode"); return; }
            if (e.name == "ISO_8859_1") { es.ctx.needText = true; es.w("Encoding.Latin1"); return; }
            if (e.name == "US_ASCII") { es.ctx.needText = true; es.w("Encoding.ASCII"); return; }
        }
        if (mappedBase == "Charset") {
            if (e.name == "forName") return;
        }
    }
    emitExpr(es, *e.obj, P_POST);
    es.w("." + csIdent(e.name));
}

void emitAssignExpr(ES& es, Expr& e, int prec) {
    TypeInfo targetType;
    if (e.a) targetType = inferExprType(es, *e.a);
    if (prec > 0) es.w("(");
    if (e.a) emitExpr(es, *e.a, P_PRIMARY);
    es.w(" " + e.op + " ");
    if (e.b) emitExpr(es, *e.b, 0, targetType);
    if (prec > 0) es.w(")");
    (void)prec;
}

void emitTernaryExpr(ES& es, Expr& e, int prec) {
    if (prec > 0) es.w("(");
    if (e.a) emitExpr(es, *e.a, 0);
    es.w(" ? ");
    TypeInfo tb = e.b ? inferExprType(es, *e.b) : TypeInfo();
    TypeInfo tc = e.c ? inferExprType(es, *e.c) : TypeInfo();
    bool bNullable = tb.known && endsWith(tb.name, "?");
    bool cNullable = tc.known && endsWith(tc.name, "?");
    if (e.b) {
        if (e.b->k == EK::Lit && e.b->lt == TT::Null &&
            ((tc.known && es.typeParams.count(tc.name) > 0) ||
             (tb.known && es.typeParams.count(tb.name) > 0))) {
            es.w("default");
        } else if (bNullable && !cNullable && tc.known && tc.name != "null" && tc.name != "object") {
            es.w("(");
            emitExpr(es, *e.b, P_POST);
            es.w(".Value)");
        } else {
            emitExpr(es, *e.b, 0);
        }
    }
    es.w(" : ");
    if (e.c) {
        if (e.c->k == EK::Lit && e.c->lt == TT::Null && tb.known &&
            es.typeParams.count(tb.name) > 0) {
            es.w("default");
        } else if (cNullable && !bNullable && tb.known && tb.name != "null" && tb.name != "object") {
            es.w("(");
            emitExpr(es, *e.c, P_POST);
            es.w(".Value)");
        } else {
            emitExpr(es, *e.c, 0);
        }
    }
    if (prec > 0) es.w(")");
}

void emitBinExpr(ES& es, Expr& e, int prec) {
    int myPrec = binPrec(e.op);
    bool paren = myPrec < prec;
    if (paren) es.w("(");
    if (e.a) {
        TypeInfo ta = inferExprType(es, *e.a);
        TypeInfo tb = e.b ? inferExprType(es, *e.b) : TypeInfo();
        bool strCtx = (tb.known && tb.name == "string") || (ta.known && ta.name == "string");
        if (e.op == "+" && ta.known && ta.name == "bool" && (strCtx || (tb.known && tb.name == "bool"))) {
            es.w("(");
            emitExpr(es, *e.a, P_EQ);
            es.w(" ? \"true\" : \"false\")");
        } else {
            emitExpr(es, *e.a, myPrec);
        }
    }
    es.w(" " + e.op + " ");
    if (e.b) {
        TypeInfo ta = e.a ? inferExprType(es, *e.a) : TypeInfo();
        TypeInfo tb = inferExprType(es, *e.b);
        bool strCtx = ta.known && ta.name == "string";
        if (e.op == "+" && tb.known && tb.name == "bool" && (strCtx || (ta.known && ta.name == "bool"))) {
            es.w("(");
            emitExpr(es, *e.b, P_EQ);
            es.w(" ? \"true\" : \"false\")");
        } else {
            emitExpr(es, *e.b, myPrec + 1);
        }
    }
    if (paren) es.w(")");
}

void emitUnExpr(ES& es, Expr& e, int prec) {
    bool paren = P_UNARY < prec;
    if (paren) es.w("(");
    es.w(e.op);
    if (e.a) emitExpr(es, *e.a, P_UNARY);
    if (paren) es.w(")");
}

void emitPostExpr(ES& es, Expr& e, int prec) {
    bool paren = P_POST < prec;
    if (paren) es.w("(");
    if (e.a) emitExpr(es, *e.a, P_POST);
    es.w(e.op);
    if (paren) es.w(")");
}

void emitCastExpr(ES& es, Expr& e, int prec, const TypeInfo& expect) {
    std::string t = csTypeStr(es.ctx, e.ty);
    std::string prim = stripNullable(t);
    if (expect.known && prim != t && expect.name == prim) t = prim;
    bool paren = P_UNARY < prec;
    if (paren) es.w("(");
    es.w("(" + t + ")");
    if (e.a) emitExpr(es, *e.a, P_POST);
    if (paren) es.w(")");
}

void emitInstOfExpr(ES& es, Expr& e, int prec) {
    bool paren = P_REL < prec;
    if (paren) es.w("(");
    if (e.a) emitExpr(es, *e.a, P_REL);
    es.w(" is " + csTypeStr(es.ctx, e.ty, true));
    if (!e.patName.empty()) es.w(" " + e.patName);
    else if (!e.patComps.empty()) {
        es.w("(");
        for (size_t i = 0; i < e.patComps.size(); i++) {
            if (i) es.w(", ");
            es.w(csTypeStr(es.ctx, e.patComps[i].first, true) + " " + e.patComps[i].second);
        }
        es.w(")");
    }
    if (paren) es.w(")");
}

void emitNewExpr(ES& es, Expr& e, const TypeInfo& expect) {
    if (e.anon) {
        es.w("new " + e.ty.name);
        return;
    }
    std::string mapped = csTypeStr(es.ctx, e.ty, true);
    if (mapped == "BigInteger" && e.args.size() == 1 &&
        inferExprType(es, *e.args[0]).name == "string") {
        es.w("BigInteger.Parse(");
        emitExpr(es, *e.args[0], 0);
        es.w(")");
        return;
    }
    if ((mapped == "decimal" || mapped == "BigDecimal") && e.args.size() == 1 &&
        inferExprType(es, *e.args[0]).name == "string") {
        es.ctx.needGlobalization = true;
        es.w("decimal.Parse(");
        emitExpr(es, *e.args[0], 0);
        es.w(", System.Globalization.CultureInfo.InvariantCulture)");
        return;
    }
    if (mapped == "__Scanner" && e.args.size() == 1 &&
        inferExprType(es, *e.args[0]).name == "string") {
        es.w("new __Scanner(new System.IO.StringReader(");
        emitExpr(es, *e.args[0], 0);
        es.w("))");
        return;
    }
    es.w("new ");
    if (e.ty.diamond) {
        bool expOk = expect.known && expect.name != "void" && expect.name != "null";
        if (expOk && !expect.args.empty()) {
            std::string tn = expect.name;
            es.w(tn + "<");
            for (size_t i = 0; i < expect.args.size(); i++) {
                if (i) es.w(", ");
                es.w(expect.args[i].text());
            }
            es.w(">");
        } else if (expOk) {
            es.w(expect.name);
        } else {
            if (mapped == "List" || mapped == "HashSet") es.w(mapped + "<object>");
            else if (mapped == "Dictionary") es.w("Dictionary<object, object>");
            else if (mapped == "SortedDictionary") es.w("SortedDictionary<object, object>");
            else if (mapped == "SortedSet") es.w("SortedSet<object>");
            else es.w(mapped);
        }
    } else {
        es.w(mapped);
    }
    es.w("(");
    emitArgs(es, e.args);
    es.w(")");
}

void emitNewArrExpr(ES& es, Expr& e, const TypeInfo& expect) {
    TypeRef elementType = e.ty;
    int trailingDims = elementType.dims;
    elementType.dims = 0;
    std::string base = csTypeStr(es.ctx, elementType, true);
    if (e.implArr && e.args.empty() && e.dims.empty()) {
        if (expect.known && expect.dims > 0) {
            TypeInfo element = expect;
            element.dims--;
            es.w("new " + element.text() + "[0]");
        } else {
            es.w("new object[0]");
        }
        return;
    }
    if (e.implArr && !e.args.empty() && e.ty.name.empty()) {
        if (expect.known && expect.dims >= 1) {
            es.w("new " + expect.text() + " { ");
            TypeInfo et = expect;
            et.dims--;
            for (size_t i = 0; i < e.args.size(); i++) {
                if (i) es.w(", ");
                emitExpr(es, *e.args[i], 0, et);
            }
            es.w(" }");
            return;
        }
        es.w("{ ");
        for (size_t i = 0; i < e.args.size(); i++) {
            if (i) es.w(", ");
            TypeInfo et2 = expect.known && !expect.args.empty() ? expect.args[0] : TypeInfo();
            emitExpr(es, *e.args[i], 0, et2);
        }
        es.w(" }");
        return;
    }
    if (!e.dims.empty() && e.dims.size() >= 2) {
        es.ctx.needLinq = true;
        for (size_t i = 0; i + 1 < e.dims.size(); i++) {
            es.w("Enumerable.Range(0, ");
            emitExpr(es, *e.dims[i], 0);
            es.w(").Select(__jdim" + std::to_string(i) + " => ");
        }
        es.w("new " + base + "[");
        emitExpr(es, *e.dims.back(), 0);
        es.w("]");
        for (int i = 0; i < trailingDims; i++) es.w("[]");
        for (size_t i = 1; i < e.dims.size(); i++) es.w(").ToArray()");
        return;
    }
    if (!e.dims.empty()) {
        es.w("new " + base + "[");
        emitExpr(es, *e.dims[0], 0);
        es.w("]");
        for (int i = 0; i < trailingDims; i++) es.w("[]");
        return;
    }
    if (!e.args.empty()) {
        es.w("new " + base);
        for (int i = 0; i < trailingDims; i++) es.w("[]");
        es.w(" { ");
        for (size_t i = 0; i < e.args.size(); i++) {
            if (i) es.w(", ");
            TypeInfo et = toInfo(es.ctx, e.ty);
            et.dims -= 1;
            et.known = !e.ty.name.empty();
            emitExpr(es, *e.args[i], 0, et);
        }
        es.w(" }");
        return;
    }
    es.w("new " + base);
    for (int i = 0; i < trailingDims; i++) es.w("[]");
}

void emitLambdaExpr(ES& es, Expr& e) {
    es.w("(");
    for (size_t i = 0; i < e.lparams.size(); i++) {
        if (i) es.w(", ");
        if (e.lparams[i].first.isVar) es.w(csIdent(e.lparams[i].second));
        else if (!e.lparams[i].first.empty()) {
            es.w(csTypeStr(es.ctx, e.lparams[i].first) + " " + csIdent(e.lparams[i].second));
        } else es.w(csIdent(e.lparams[i].second));
    }
    es.w(") => ");
    if (e.lbodyIsBlock) {
        es.w("{");
        es.nl();
        es.indent++;
        std::vector<VarInfo> pushed;
        for (auto& lp : e.lparams) {
            VarInfo vi;
            vi.name = lp.second;
            if (!lp.first.empty() && !lp.first.isVar) vi.ty = toInfo(es.ctx, lp.first);
            es.scope.push_back(vi);
            pushed.push_back(vi);
        }
        for (auto& s : e.lblock) emitStmt(es, *s);
        for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
        es.indent--;
        es.wi();
        es.w("}");
    } else {
        if (e.lbody) emitExpr(es, *e.lbody, 0);
    }
}

void emitMRefExpr(ES& es, Expr& e, const TypeInfo& expect) {
    std::string path = e.obj ? pathText(*e.obj) : "";
    if (path == "System.out") {
        es.w("Console.WriteLine");
        return;
    }
    if (path == "System.err") {
        es.w("Console.Error.WriteLine");
        return;
    }
    if (e.name == "new") {
        TypeInfo dgt = expect;
        if (dgt.known && startsWith(dgt.name, "Func") && dgt.args.size() >= 1) {
            size_t n = dgt.args.size() - 1;
            es.w("(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(") => new ");
            emitExpr(es, *e.obj, P_PRIMARY);
            es.w("(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(")");
            return;
        }
        es.w("__m0 => new ");
        emitExpr(es, *e.obj, P_PRIMARY);
        es.w("(__m0)");
        return;
    }
    if (e.obj && e.obj->k == EK::Ident) {
        std::string tn = e.obj->name;
        std::string cn = mapJavaTypeFqn(tn, true);
        std::string cc = cn.empty() ? tn : cn;
        std::string staticArity;
        {
            const std::string prefix = tn + "." + e.name + "/";
            for (const auto& sm : es.ctx.staticMethods) {
                if (startsWith(sm, prefix)) {
                    staticArity = sm.substr(prefix.size());
                    break;
                }
            }
        }
        if (!staticArity.empty()) {
            size_t n = 1;
            try {
                n = (size_t)std::stoi(staticArity);
            } catch (...) {
                n = 1;
            }
            es.w("(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(") => " + tn + "." + csIdent(e.name) + "(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(")");
            return;
        }
        if (cc == "Math") {
            es.w("Math." + staticMathName(e.name));
            return;
        }
        if ((cc == "int" || cc == "long" || cc == "double" || cc == "float" || cc == "bool") &&
            (e.name == "sum" || e.name == "max" || e.name == "min" || e.name == "compare")) {
            if (e.name == "sum") {
                es.w("((__a, __b) => __a + __b)");
                return;
            }
            if (e.name == "max" || e.name == "min") {
                es.w(std::string("Math.") + (e.name == "max" ? "Max" : "Min"));
                return;
            }
            es.w("((__a, __b) => __a.CompareTo(__b))");
            return;
        }
        if ((cc == "int" && e.name == "parseInt") || (cc == "long" && e.name == "parseLong") ||
            (cc == "double" && e.name == "parseDouble") ||
            (cc == "float" && e.name == "parseFloat") ||
            (cc == "bool" && e.name == "parseBoolean")) {
            es.w(cc + ".Parse");
            return;
        }
        if (cc == "string" || cc == "String") {
            static const std::map<std::string, std::string> sm = {
                {"toUpperCase", "ToUpper()"}, {"toLowerCase", "ToLower()"},
                {"trim", "Trim()"}, {"strip", "Trim()"}};
            auto it = sm.find(e.name);
            if (it != sm.end()) {
                es.w("(__mr => __mr." + it->second + ")");
                return;
            }
            if (e.name == "length") {
                es.w("(__mr => __mr.Length)");
                return;
            }
            if (e.name == "isEmpty") {
                es.w("(__mr => __mr.Length == 0)");
                return;
            }
            if (e.name == "compareTo") {
                es.w("((__m0, __m1) => __m0.CompareTo(__m1))");
                return;
            }
            if (e.name == "format") { es.w("string.Format"); return; }
            if (e.name == "join") { es.w("string.Join"); return; }
            if (e.name == "valueOf") { es.w("Convert.ToString"); return; }
        }
        if (cc == "Objects") {
            if (e.name == "equals") { es.w("object.Equals"); return; }
            if (e.name == "isNull") { es.w("(__mr => __mr == null)"); return; }
            if (e.name == "nonNull") { es.w("(__mr => __mr != null)"); return; }
        }
        if (es.ctx.typeByName.count(tn) && !es.ctx.samDelegates.count(tn)) {
            auto& rets = es.ctx.methodReturns[tn];
            size_t nArgs = 1;
            if (expect.known && startsWith(expect.name, "Func") && expect.args.size() >= 2)
                nArgs = expect.args.size() - 2;
            bool have = rets.count(e.name + "/" + std::to_string(nArgs)) > 0 ||
                        (nArgs == 1 && rets.count(e.name + "/0") > 0);
            if (have) {
                if (nArgs == 0) {
                    es.w("(__mr => __mr." + csIdent(e.name) + "())");
                } else {
                    es.w("(");
                    for (size_t i = 0; i < nArgs; i++)
                        es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
                    es.w(") => __m0." + csIdent(e.name) + "(");
                    for (size_t i = 1; i < nArgs; i++)
                        es.w(i == 1 ? "__m" + std::to_string(i) : ", __m" + std::to_string(i));
                    es.w(")");
                }
                return;
            }
        }
        if (es.ctx.typeByName.count(tn)) {
            es.w(tn + "." + csIdent(e.name));
            return;
        }
    }
    if (e.obj) emitExpr(es, *e.obj, P_POST);
    es.w("." + e.name);
}

void emitSwitchEExpr(ES& es, Expr& e, const TypeInfo& expect) {
    TypeInfo subjT = inferExprType(es, *e.a);
    std::string subjName = subjT.known ? subjT.name : "";
    es.w(captureExprText(es, *e.a, P_PRIMARY));
    es.w(" switch");
    es.nl();
    es.wi();
    es.w("{");
    es.nl();
    es.indent++;
    for (auto& arm : e.cases) {
        es.wi();
        bool first = true;
        if (arm.isDefault) {
            es.w("_");
            first = false;
        }
        if (arm.nullLabel) {
            es.w("null");
            first = false;
        }
        for (auto& pl : arm.patLabels) {
            if (!first) es.w(" or ");
            first = false;
            es.w(csTypeStr(es.ctx, pl, true) + (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)));
        }
        for (auto& l : arm.labels) {
            if (!first) es.w(" or ");
            first = false;
            es.w(enumLabelText(es, *l, subjName));
        }
        if (first) es.w("_");
        if (arm.guard) es.w(" when " + captureExprText(es, *arm.guard, 0));
        es.w(" => ");
        if (arm.value) emitExpr(es, *arm.value, 0, expect);
        else {
            Expr* yieldVal = nullptr;
            for (auto& s : arm.body)
                if (s->k == SK::Return && s->isYield && s->e) yieldVal = s->e.get();
            if (yieldVal) emitExpr(es, *yieldVal, 0, expect);
            else es.w("default");
        }
        es.w(",");
        es.nl();
    }
    es.indent--;
    es.wi();
    es.w("}");
}

void emitCtorCallExpr(ES& es, Expr& e) {
    es.w(e.name == "super" ? "base" : "this");
    es.w("(");
    emitArgs(es, e.args);
    es.w(")");
}

void emitClassLitExpr(ES& es, Expr& e) {
    es.w("typeof(" + csTypeStr(es.ctx, e.ty, true) + ")");
}

void emitCall(ES& es, Expr& e, const TypeInfo& expect) {
    std::string path = e.obj ? pathText(*e.obj) : "";
    if (!e.obj) {
        if (es.ctx.staticImports.count(e.name)) {
            es.w(es.ctx.staticImports[e.name] + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (es.ctx.needStaticMath) {
            std::string mn = staticMathName(e.name);
            if (e.name == "round") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "float") {
                    es.w("(int)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                } else {
                    es.w("(long)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                }
                return;
            }
            if (e.name == "random") {
                es.w("Random.Shared.NextDouble()");
                return;
            }
            if (e.name == "toRadians") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * Math.PI / 180.0)");
                return;
            }
            if (e.name == "toDegrees") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * 180.0 / Math.PI)");
                return;
            }
            if (mn != e.name) {
                es.w("Math." + mn + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        es.w(e.name + "(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    if (path == "System.out") {
        if (e.name == "println") {
            if (e.args.size() == 1) {
                TypeInfo at = inferExprType(es, *e.args[0]);
                if (at.known && at.name == "bool") {
                    es.w("Console.WriteLine((");
                    emitExpr(es, *e.args[0], 0);
                    es.w(") ? \"true\" : \"false\")");
                    return;
                }
            }
            es.w("Console.WriteLine(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "print") {
            if (e.args.size() == 1) {
                TypeInfo at = inferExprType(es, *e.args[0]);
                if (at.known && at.name == "bool") {
                    es.w("Console.Write((");
                    emitExpr(es, *e.args[0], 0);
                    es.w(") ? \"true\" : \"false\")");
                    return;
                }
            }
            es.w("Console.Write(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "printf") {
            FmtXlate fx;
            if (!e.args.empty() && isStringLit(*e.args[0])) fx = translatePrintfFmt(e.args[0]->val);
            es.w("Console.Write(string.Format(\"" + escapeCsStr(fx.fmt) + "\"");
            for (size_t i = 1; i < e.args.size(); i++) {
                es.w(", ");
                emitFmtArg(es, *e.args[i], i - 1 < fx.tags.size() ? fx.tags[i - 1] : 0);
            }
            es.w("))");
            return;
        }
        if (e.name == "flush") {
            es.w("Console.Out.Flush()");
            return;
        }
        es.w("Console.WriteLine(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    if (path == "System.err") {
        if (e.name == "println") {
            es.w("Console.Error.WriteLine(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "print") {
            es.w("Console.Error.Write(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        es.w("Console.Error.WriteLine(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    std::string objClass = e.obj ? lastSeg(pathText(*e.obj)) : "";
    if (objClass == "System" && e.obj && e.obj->k == EK::Ident) {
        if (e.name == "exit") {
            es.w("Environment.Exit(");
            if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
        if (e.name == "currentTimeMillis") {
            es.w("DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()");
            return;
        }
        if (e.name == "nanoTime") {
            es.ctx.needDiag = true;
            es.w("(long)(Stopwatch.GetTimestamp() * 1e9 / Stopwatch.Frequency)");
            return;
        }
        if (e.name == "getProperty") {
            es.w("Environment.GetEnvironmentVariable(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "lineSeparator") {
            es.w("Environment.NewLine");
            return;
        }
        if (e.name == "arraycopy") {
            es.w("Array.Copy(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "getenv") {
            es.w("Environment.GetEnvironmentVariables()");
            return;
        }
    }
    if (e.obj && (e.obj->k == EK::Ident || e.obj->k == EK::Field)) {
        std::string clsName = objClass;
        std::string mappedCls = mapJavaTypeFqn(clsName, true);
        std::string canon = mappedCls.empty() ? clsName : mappedCls;
        if (clsName == "Stream" || clsName == "IntStream" || clsName == "LongStream" ||
            clsName == "DoubleStream") {
            if (e.name == "of") {
                std::string elementType;
                if (!e.targs.empty()) elementType = csTypeStr(es.ctx, e.targs.front(), true);
                else if (expect.known && !expect.args.empty()) elementType = expect.args[0].text();
                else if (clsName == "IntStream") elementType = "int";
                else if (clsName == "LongStream") elementType = "long";
                else if (clsName == "DoubleStream") elementType = "double";
                else if (!e.args.empty()) {
                    TypeInfo element = inferExprType(es, *e.args.front());
                    if (element.known) elementType = element.text();
                }
                if (elementType.empty()) elementType = "object";
                es.ctx.needLinq = true;
                if (e.args.empty()) {
                    es.w("Enumerable.Empty<" + elementType + ">()");
                } else {
                    es.w("new " + elementType + "[] { ");
                    emitArgs(es, e.args);
                    es.w(" }.AsEnumerable()");
                }
                return;
            }
            if (e.name == "empty") {
                es.ctx.needLinq = true;
                std::string elementType;
                if (!e.targs.empty()) elementType = csTypeStr(es.ctx, e.targs.front(), true);
                else if (expect.known && !expect.args.empty()) elementType = expect.args[0].text();
                else if (clsName == "IntStream") elementType = "int";
                else if (clsName == "LongStream") elementType = "long";
                else if (clsName == "DoubleStream") elementType = "double";
                else elementType = "object";
                es.w("Enumerable.Empty<" + elementType + ">()");
                return;
            }
            if (e.name == "range" || e.name == "rangeClosed") {
                es.ctx.needLinq = true;
                if (clsName == "IntStream" || clsName == "LongStream") {
                    es.ctx.needCollectionsSpec = true;
                    es.w(std::string("__JHelp.") +
                         (clsName == "LongStream" ? "LongRange(" : "IntRange("));
                    if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                    else es.w("0");
                    es.w(", ");
                    if (e.args.size() > 1) emitExpr(es, *e.args[1], 0);
                    else es.w("0");
                    es.w(e.name == "rangeClosed" ? ", true)" : ", false)");
                } else {
                    es.w("Enumerable.Range(");
                    if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                    es.w(", ");
                    if (e.args.size() > 1) {
                        if (e.name == "rangeClosed") es.w("(");
                        emitExpr(es, *e.args[1], 0);
                        es.w(" - ");
                        emitExpr(es, *e.args[0], 0);
                        if (e.name == "rangeClosed") es.w(" + 1)");
                    } else es.w("0");
                    es.w(")");
                }
                return;
            }
        }
        if (canon == "Math" || clsName == "StrictMath") {
            std::string n = staticMathName(e.name);
            if (e.name == "round") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "float") {
                    es.w("(int)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                } else {
                    es.w("(long)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                }
                return;
            }
            if (e.name == "random") {
                es.w("Random.Shared.NextDouble()");
                return;
            }
            if (e.name == "toRadians") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * Math.PI / 180.0)");
                return;
            }
            if (e.name == "toDegrees") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * 180.0 / Math.PI)");
                return;
            }
            if (e.name == "floorDiv" || e.name == "floorMod") {
                es.w("Math." + n + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "abs" || e.name == "max" || e.name == "min") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "decimal") {
                    es.w("Math." + n + "(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (at.known && at.name == "long") {
                    es.w("Math." + n + "(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
            }
            es.w("Math." + n + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (canon == "int" || canon == "Integer" || canon == "long" || canon == "Long" ||
            canon == "short" || canon == "Short" || canon == "byte" || canon == "Byte") {
            std::string prim = canon == "int" || canon == "Integer" ? "int"
                               : canon == "long" || canon == "Long"  ? "long"
                               : canon == "short" || canon == "Short" ? "short"
                                                       : "byte";
            if (e.name == "parseInt" || e.name == "parseLong" || e.name == "parseShort" ||
                e.name == "parseByte") {
                es.w(prim + ".Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                if (e.args.size() == 1) {
                    TypeInfo at = inferExprType(es, *e.args[0]);
                    if (at.known && at.name == "string") {
                        es.w(prim + ".Parse(");
                        emitExpr(es, *e.args[0], 0);
                        es.w(")");
                    } else {
                        emitExpr(es, *e.args[0], 0);
                    }
                    return;
                }
            }
            if (e.name == "toString") {
                if (!e.args.empty()) {
                    TypeInfo at = inferExprType(es, *e.args[0]);
                    if (at.known && isNumericCs(at.name)) {
                        es.w("(");
                        emitExpr(es, *e.args[0], P_POST);
                        es.w(").ToString()");
                    } else {
                        es.w("Convert.ToString(");
                        emitArgs(es, e.args);
                        es.w(")");
                    }
                    return;
                }
            }
            if (e.name == "max" || e.name == "min") {
                es.w(std::string("Math.") + (e.name == "max" ? "Max" : "Min") + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "compare") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").CompareTo(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "sum") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" + ");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "toBinaryString") {
                es.w("Convert.ToString(");
                emitExpr(es, *e.args[0], 0);
                es.w(", 2)");
                return;
            }
            if (e.name == "toHexString") {
                es.w("Convert.ToString(");
                emitExpr(es, *e.args[0], 0);
                es.w(", 16)");
                return;
            }
            if (e.name == "toOctalString") {
                es.w("Convert.ToString(");
                emitExpr(es, *e.args[0], 0);
                es.w(", 8)");
                return;
            }
            if (e.name == "bitCount") {
                es.ctx.needNumerics = true;
                es.w("System.Numerics.BitOperations.PopCount((uint)(");
                emitExpr(es, *e.args[0], 0);
                es.w("))");
                return;
            }
            if (e.name == "hashCode") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").GetHashCode()");
                return;
            }
        }
        if (canon == "double" || canon == "Double" || canon == "float" || canon == "Float") {
            std::string prim = canon;
            if (e.name == "parseDouble" || e.name == "parseFloat") {
                es.w(prim + ".Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "string") {
                    es.w(prim + ".Parse(");
                    emitExpr(es, *e.args[0], 0);
                    es.w(")");
                } else if (!e.args.empty()) {
                    emitExpr(es, *e.args[0], 0);
                }
                return;
            }
            if (e.name == "isNaN") {
                es.w(prim + ".IsNaN(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "isInfinite") {
                es.w(prim + ".IsInfinity(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "isFinite") {
                es.w(prim + ".IsFinite(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "compare") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").CompareTo(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "toString") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").ToString()");
                return;
            }
        }
        if (canon == "bool" || canon == "Boolean") {
            if (e.name == "parseBoolean") {
                es.w("bool.Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "string") {
                    es.w("bool.Parse(");
                    emitExpr(es, *e.args[0], 0);
                    es.w(")");
                } else if (!e.args.empty()) {
                    emitExpr(es, *e.args[0], 0);
                }
                return;
            }
            if (e.name == "toString") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").ToString()");
                return;
            }
            if (e.name == "compare") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").CompareTo(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
        }
        if (canon == "char" || canon == "Character") {
            static const std::map<std::string, std::string> charM = {
                {"isDigit", "IsDigit"}, {"isLetter", "IsLetter"},
                {"isLetterOrDigit", "IsLetterOrDigit"}, {"isWhitespace", "IsWhiteSpace"},
                {"isUpperCase", "IsUpper"}, {"isLowerCase", "IsLower"},
                {"toUpperCase", "ToUpper"}, {"toLowerCase", "ToLower"},
                {"isAlphabetic", "IsLetter"}, {"isSpaceChar", "IsWhiteSpace"}};
            auto cm = charM.find(e.name);
            if (cm != charM.end()) {
                es.w("char." + cm->second + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "getNumericValue") {
                es.w("((int)char.GetNumericValue(");
                emitArgs(es, e.args);
                es.w("))");
                return;
            }
            if (e.name == "toString") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").ToString()");
                return;
            }
        }
        if (canon == "string" || canon == "String") {
            if (e.name == "format") {
                FmtXlate fx;
                if (!e.args.empty() && isStringLit(*e.args[0])) fx = translatePrintfFmt(e.args[0]->val);
                es.w("string.Format(\"" + escapeCsStr(fx.fmt) + "\"");
                for (size_t i = 1; i < e.args.size(); i++) {
                    es.w(", ");
                    emitFmtArg(es, *e.args[i], i - 1 < fx.tags.size() ? fx.tags[i - 1] : 0);
                }
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                if (!e.args.empty()) {
                    TypeInfo at = inferExprType(es, *e.args[0]);
                    if (at.known && at.dims > 0 && at.name == "char") {
                        es.w("new string(");
                        emitExpr(es, *e.args[0], 0);
                        es.w(")");
                    } else {
                        es.w("Convert.ToString(");
                        emitExpr(es, *e.args[0], 0);
                        es.w(")");
                    }
                    return;
                }
            }
            if (e.name == "join") {
                es.w("string.Join(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "isEmpty") {
                es.w("string.IsNullOrEmpty(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "Objects") {
            if (e.name == "equals") {
                es.w("object.Equals(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "requireNonNull") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" ?? throw new ArgumentNullException())");
                return;
            }
            if (e.name == "requireNonNullElse") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" ?? ");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "isNull") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" == null)");
                return;
            }
            if (e.name == "nonNull") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" != null)");
                return;
            }
            if (e.name == "hash") {
                es.w("System.HashCode.Combine(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "toString") {
                es.w("Convert.ToString(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "List" || clsName == "List" || clsName == "ArrayList") {
            if (e.name == "of") {
                TypeInfo et;
                bool haveEt = false;
                if (expect.known && !expect.args.empty()) {
                    et = expect.args[0];
                    haveEt = true;
                } else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known && a0.name != "null") {
                        et = a0;
                        haveEt = true;
                    }
                }
                es.w("new List<" + (haveEt ? et.text() : "object") + "> { ");
                emitArgs(es, e.args);
                es.w(" }");
                return;
            }
        }
        if (clsName == "Set" || clsName == "HashSet") {
            if (e.name == "of") {
                TypeInfo et;
                bool haveEt = false;
                if (expect.known && !expect.args.empty()) {
                    et = expect.args[0];
                    haveEt = true;
                } else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known && a0.name != "null") {
                        et = a0;
                        haveEt = true;
                    }
                }
                es.w("new HashSet<" + (haveEt ? et.text() : "object") + "> { ");
                emitArgs(es, e.args);
                es.w(" }");
                return;
            }
        }
        if (clsName == "Map" || clsName == "HashMap") {
            if (e.name == "of" && e.args.size() % 2 == 0) {
                TypeInfo keyType;
                TypeInfo valueType;
                if (expect.known && expect.args.size() >= 2) {
                    keyType = expect.args[0];
                    valueType = expect.args[1];
                } else if (e.args.size() >= 2) {
                    keyType = inferExprType(es, *e.args[0]);
                    valueType = inferExprType(es, *e.args[1]);
                }
                std::string keyName = keyType.known ? keyType.text() : "object";
                std::string valueName = valueType.known ? valueType.text() : "object";
                es.w("new Dictionary<" + keyName + ", " + valueName + "> { ");
                for (size_t i = 0; i + 1 < e.args.size(); i += 2) {
                    if (i) es.w(", ");
                    es.w("{ ");
                    emitExpr(es, *e.args[i], 0, keyType);
                    es.w(", ");
                    emitExpr(es, *e.args[i + 1], 0, valueType);
                    es.w(" }");
                }
                es.w(" }");
                return;
            }
        }
        if (es.ctx.simpleEnums.count(clsName) || es.ctx.enumClassTypes.count(clsName)) {
            if (e.name == "values") {
                if (es.ctx.simpleEnums.count(clsName))
                    es.w("((" + clsName + "[])Enum.GetValues(typeof(" + clsName + ")))");
                else
                    es.w("(" + clsName + ".Values())");
                return;
            }
            if (e.name == "valueOf") {
                if (es.ctx.simpleEnums.count(clsName)) {
                    es.w("((" + clsName + ")Enum.Parse(typeof(" + clsName + "), ");
                    if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                    es.w("))");
                } else {
                    es.w("(" + clsName + ".ValueOf(");
                    emitArgs(es, e.args);
                    es.w("))");
                }
                return;
            }
        }
        if (clsName == "Files" || clsName == "Paths") {
            es.ctx.needIO = true;
            if (clsName == "Paths" && e.name == "get") {
                if (e.args.size() == 1) {
                    emitExpr(es, *e.args[0], 0);
                } else {
                    es.w("Path.Combine(");
                    emitArgs(es, e.args);
                    es.w(")");
                }
                return;
            }
            if (clsName == "Files" && e.name == "exists") {
                es.w("File.Exists(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "delete") {
                es.w("File.Delete(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "readAllLines") {
                es.w("File.ReadAllLines(");
                emitArgs(es, e.args);
                es.w(").ToList()");
                return;
            }
            if (clsName == "Files" && e.name == "readAllBytes") {
                es.w("File.ReadAllBytes(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "write") {
                es.w("File.WriteAllBytes(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "createTempFile") {
                es.w("Path.Combine(Path.GetTempPath(), ");
                if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                else es.w("\"tmp\"");
                es.w(" + \"-\" + Guid.NewGuid().ToString(\"N\") + ");
                if (e.args.size() > 1) emitExpr(es, *e.args[1], 0);
                else es.w("\".tmp\"");
                es.w(")");
                return;
            }
        }
        if (canon == "Comparison" || clsName == "Comparator") {
            if (e.name == "naturalOrder") {
                es.w("((a, b) => Comparer<object>.Default.Compare(a, b))");
                return;
            }
            if (e.name == "reverseOrder") {
                es.w("((a, b) => Comparer<object>.Default.Compare(b, a))");
                return;
            }
        }
        if (canon == "Array" || canon == "Arrays") {
            std::string n = arraysStaticName(e.name);
            if (e.name == "asList") {
                TypeInfo et;
                bool haveEt = false;
                if (expect.known && !expect.args.empty()) {
                    et = expect.args[0];
                    haveEt = true;
                } else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known && a0.dims == 0 && a0.name != "null") {
                        et = a0;
                        haveEt = true;
                    }
                }
                if (haveEt) {
                    es.w("new List<" + et.text() + "> { ");
                    for (size_t i = 0; i < e.args.size(); i++) {
                        if (i) es.w(", ");
                        emitExpr(es, *e.args[i], 0, et);
                    }
                    es.w(" }");
                } else {
                    es.w("new List<object> { ");
                    emitArgs(es, e.args);
                    es.w(" }");
                }
                return;
            }
            if (e.name == "sort") {
                if (e.args.size() == 1) {
                    es.w("Array.Sort(" + captureExprText(es, *e.args[0], P_POST) + ")");
                    return;
                }
                if (e.args.size() == 2) {
                    es.w("Array.Sort(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                    emitExpr(es, *e.args[1], 0);
                    es.w(")");
                    return;
                }
                if (e.args.size() == 3) {
                    es.w("Array.Sort(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                    emitExpr(es, *e.args[1], 0);
                    es.w(", (" + captureExprText(es, *e.args[2], 0) + ") - (" +
                         captureExprText(es, *e.args[1], 0) + "))");
                    return;
                }
            }
            if (e.name == "fill") {
                es.w("Array.Fill(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "copyOf") {
                es.ctx.needCollectionsSpec = true;
                es.w("__JHelp.CopyOf(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "copyOfRange") {
                es.ctx.needCollectionsSpec = true;
                es.w("__JHelp.CopyOfRange(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                emitExpr(es, *e.args[1], 0);
                es.w(", ");
                emitExpr(es, *e.args[2], 0);
                es.w(")");
                return;
            }
            if (e.name == "toString" || e.name == "deepToString") {
                es.ctx.needLinq = true;
                es.w("$\"[{string.Join(\", \", ");
                emitExpr(es, *e.args[0], 0);
                es.w(")}]\"");
                return;
            }
            if (e.name == "equals") {
                es.ctx.needLinq = true;
                es.w(captureExprText(es, *e.args[0], P_POST) + ".SequenceEqual(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "binarySearch") {
                es.w("Array.BinarySearch(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "hashCode") {
                es.w("Array.Empty<int>().GetHashCode()");
                return;
            }
            es.w("Array." + n + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (canon == "Collections") {
            if (e.name == "sort") {
                if (!e.args.empty()) {
                    es.w(captureExprText(es, *e.args[0], P_POST) + ".Sort(");
                    for (size_t i = 1; i < e.args.size(); i++) {
                        if (i > 1) es.w(", ");
                        emitExpr(es, *e.args[i], 0);
                    }
                    es.w(")");
                    return;
                }
            }
            if (e.name == "reverse") {
                if (!e.args.empty()) {
                    es.ctx.needLinq = true;
                    es.w(captureExprText(es, *e.args[0], P_POST) + " = " +
                         captureExprText(es, *e.args[0], P_POST) + ".AsEnumerable().Reverse().ToList()");
                    return;
                }
            }
            if (e.name == "max" || e.name == "min") {
                es.ctx.needLinq = true;
                es.w(captureExprText(es, *e.args[0], P_POST) + "." +
                     (e.name == "max" ? "Max" : "Min") + "()");
                return;
            }
            if (e.name == "unmodifiableList" || e.name == "unmodifiableCollection") {
                if (!e.args.empty()) {
                    es.w(captureExprText(es, *e.args[0], P_POST) + ".AsReadOnly()");
                    return;
                }
            }
            if (e.name == "unmodifiableMap" || e.name == "unmodifiableSet" ||
                e.name == "singleton" || e.name == "unmodifiableSortedMap") {
                if (!e.args.empty()) {
                    es.w(captureExprText(es, *e.args[0], P_POST));
                    return;
                }
            }
            if (e.name == "emptyList") {
                if (expect.known && expect.name == "List" && !expect.args.empty())
                    es.w("new List<" + expect.args[0].text() + ">()");
                else es.w("new List<object>()");
                return;
            }
            if (e.name == "emptyMap") {
                if (expect.known && expect.name == "Dictionary" && expect.args.size() == 2)
                    es.w("new Dictionary<" + expect.args[0].text() + ", " +
                         expect.args[1].text() + ">()");
                else es.w("new Dictionary<object, object>()");
                return;
            }
            if (e.name == "emptySet") {
                if (expect.known && expect.name == "HashSet" && !expect.args.empty())
                    es.w("new HashSet<" + expect.args[0].text() + ">()");
                else es.w("new HashSet<object>()");
                return;
            }
            if (e.name == "singletonList") {
                TypeInfo et;
                if (expect.known && expect.name == "List" && !expect.args.empty()) et = expect.args[0];
                else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known) et = a0;
                }
                es.w("new List<" + (et.known ? et.text() : "object") + "> { ");
                if (!e.args.empty()) emitExpr(es, *e.args[0], 0, et);
                es.w(" }");
                return;
            }
            if (e.name == "nCopies") {
                es.ctx.needLinq = true;
                es.w("Enumerable.Repeat(");
                emitExpr(es, *e.args[1], 0);
                es.w(", ");
                emitExpr(es, *e.args[0], 0);
                es.w(").ToList()");
                return;
            }
            if (e.name == "binarySearch") {
                es.w(captureExprText(es, *e.args[0], P_POST) + ".BinarySearch(");
                if (e.args.size() > 1) emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "swap") {
                if (e.args.size() == 3) {
                    std::string l = captureExprText(es, *e.args[0], P_POST);
                    es.w("((" + l + "[" + captureExprText(es, *e.args[1], 0) + "], " + l + "[" +
                         captureExprText(es, *e.args[2], 0) + "]) = (" + l + "[" +
                         captureExprText(es, *e.args[2], 0) + "], " + l + "[" +
                         captureExprText(es, *e.args[1], 0) + "]))");
                    return;
                }
            }
        }
        if (canon == "Guid" || canon == "UUID") {
            if (e.name == "randomUUID") {
                es.w("Guid.NewGuid()");
                return;
            }
            if (e.name == "fromString") {
                es.w("Guid.Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "Thread") {
            if (e.name == "sleep") {
                es.ctx.needThreading = true;
                es.w("Thread.Sleep(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "currentThread") {
                es.w("Thread.CurrentThread");
                return;
            }
        }
        if (clsName == "LocalDate" || clsName == "LocalDateTime" || clsName == "Instant" ||
            clsName == "LocalTime" || clsName == "Duration" || clsName == "ZonedDateTime" ||
            clsName == "Period") {
            if (e.name == "now") {
                if (clsName == "LocalDate") es.w("DateOnly.FromDateTime(DateTime.Now)");
                else if (clsName == "LocalTime") es.w("TimeOnly.FromDateTime(DateTime.Now)");
                else if (clsName == "LocalDateTime") es.w("DateTime.Now");
                else if (clsName == "Instant" || clsName == "ZonedDateTime")
                    es.w("DateTimeOffset.UtcNow");
                else es.w("TimeSpan.Zero");
                return;
            }
            if (e.name == "of") {
                if (clsName == "LocalDate") {
                    es.w("new DateOnly(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (clsName == "LocalTime") {
                    es.w("new TimeOnly(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (clsName == "LocalDateTime") {
                    es.w("new DateTime(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (clsName == "Duration") {
                    static const std::map<std::string, std::string> durM = {
                        {"ofMillis", "FromMilliseconds"}, {"ofSeconds", "FromSeconds"},
                        {"ofMinutes", "FromMinutes"}, {"ofHours", "FromHours"},
                        {"ofDays", "FromDays"}};
                    if (!e.args.empty() && isStringLit(*e.args[0]) == false) {
                    }
                    es.w("TimeSpan.FromMilliseconds(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
            }
            if (e.name == "parse") {
                if (clsName == "LocalDate") es.w("DateOnly.Parse(");
                else if (clsName == "LocalTime") es.w("TimeOnly.Parse(");
                else if (clsName == "Instant") es.w("DateTimeOffset.Parse(");
                else es.w("DateTime.Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "between") {
                es.w("(");
                emitExpr(es, *e.args[1], 0);
                es.w(" - ");
                emitExpr(es, *e.args[0], 0);
                es.w(")");
                return;
            }
            if (e.name == "ofMillis" || e.name == "ofSeconds" || e.name == "ofMinutes" ||
                e.name == "ofHours" || e.name == "ofDays") {
                static const std::map<std::string, std::string> durM2 = {
                    {"ofMillis", "FromMilliseconds"}, {"ofSeconds", "FromSeconds"},
                    {"ofMinutes", "FromMinutes"}, {"ofHours", "FromHours"},
                    {"ofDays", "FromDays"}};
                es.w("TimeSpan." + durM2.at(e.name) + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "Regex" || clsName == "Pattern") {
            if (e.name == "compile") {
                es.w("new Regex(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "matches") {
                es.ctx.needRegex = true;
                es.w("Regex.IsMatch(");
                if (e.args.size() >= 2) {
                    emitExpr(es, *e.args[1], 0);
                    es.w(", ");
                    emitExpr(es, *e.args[0], 0);
                }
                es.w(")");
                return;
            }
        }
        if (clsName == "Optional") {
            if (e.name == "ofNullable" || e.name == "of") {
                if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                return;
            }
            if (e.name == "empty") {
                es.w("null");
                return;
            }
        }
        if (canon == "decimal" || clsName == "BigDecimal") {
            if (e.name == "valueOf") {
                if (!e.args.empty()) {
                    es.w(std::string("((") + (inferExprType(es, *e.args[0]).name == "long" ? "long" : "decimal") +
                         ")(");
                    emitExpr(es, *e.args[0], 0);
                    es.w("))");
                    return;
                }
            }
        }
        if (e.obj->k == EK::Ident && es.ctx.typeByName.count(clsName)) {
            Decl* cd = es.ctx.typeByName[clsName];
            auto& rets = es.ctx.methodReturns[clsName];
            auto it = rets.find(e.name + "/" + std::to_string(e.args.size()));
            (void)cd;
            (void)it;
        }
    }
    if (e.obj->k == EK::Call && e.name == "matches") {
        Expr* inner = e.obj.get();
        if (inner->name == "matcher" && inner->obj && inner->obj->k == EK::Call &&
            inner->obj->name == "compile" && inner->obj->obj &&
            lastSeg(pathText(*inner->obj->obj)) == "Pattern") {
            es.ctx.needRegex = true;
            es.w("Regex.IsMatch(");
            emitExpr(es, *inner->args[0], 0);
            es.w(", ");
            emitExpr(es, *inner->obj->args[0], 0);
            es.w(")");
            return;
        }
    }
    if (e.obj->k == EK::Call && e.name == "replaceAll") {
        Expr* inner = e.obj.get();
        if (inner->name == "matcher" && inner->obj && inner->obj->k == EK::Call &&
            inner->obj->name == "compile" && inner->obj->obj &&
            lastSeg(pathText(*inner->obj->obj)) == "Pattern") {
            es.ctx.needRegex = true;
            es.w("Regex.Replace(");
            emitExpr(es, *inner->args[0], 0);
            es.w(", ");
            emitExpr(es, *inner->obj->args[0], 0);
            es.w(", ");
            emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
    }
    TypeInfo rt;
    if (e.obj) rt = inferExprType(es, *e.obj);
    std::string recv = e.obj ? captureExprText(es, *e.obj, P_POST) : "";
    std::string rname = rt.known ? rt.name : "";
    if (rt.known && e.args.empty()) {
        auto rcit = es.ctx.recordComps.find(rname);
        if (rcit != es.ctx.recordComps.end()) {
            bool isComp = false;
            for (auto& cn : rcit->second)
                if (cn == e.name) isComp = true;
            if (isComp) {
                es.w(recv + "." + pascalCase(e.name));
                return;
            }
        }
    }
    if (e.name == "stream" && rt.known) {
        es.w(recv);
        return;
    }
    if (e.name == "boxed" && rt.known) {
        es.w(recv);
        return;
    }
    if (e.name == "entrySet" && rname == "Dictionary") {
        es.w(recv);
        return;
    }
        if (rname == "string") {
        if (e.name == "length") { es.w(recv + ".Length"); return; }
        if (e.name == "charAt") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "substring") {
            if (e.args.size() == 1) {
                es.w(recv + ".Substring(" + captureExprText(es, *e.args[0], 0) + ")");
            } else {
                es.w(recv + ".Substring(" + captureExprText(es, *e.args[0], 0) + ", " +
                     captureExprText(es, *e.args[1], 0) + " - " +
                     captureExprText(es, *e.args[0], 0) + ")");
            }
            return;
        }
        if (e.name == "indexOf") { es.w(recv + ".IndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "lastIndexOf") { es.w(recv + ".LastIndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "equals") {
            es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_ADD) + ")");
            return;
        }
        if (e.name == "equalsIgnoreCase") {
            es.w("string.Equals(" + recv + ", " + captureExprText(es, *e.args[0], 0) +
                 ", StringComparison.OrdinalIgnoreCase)");
            return;
        }
        if (e.name == "contains") { es.w(recv + ".Contains("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "toUpperCase") { es.w(recv + ".ToUpper()"); return; }
        if (e.name == "toLowerCase") { es.w(recv + ".ToLower()"); return; }
        if (e.name == "trim" || e.name == "strip") { es.w(recv + ".Trim()"); return; }
        if (e.name == "replace") { es.w(recv + ".Replace("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "replaceAll") {
            es.ctx.needRegex = true;
            es.w("Regex.Replace(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(", ");
            emitExpr(es, *e.args[1], 0);
            es.w(")");
            return;
        }
        if (e.name == "replaceFirst") {
            es.ctx.needRegex = true;
            es.w("Regex.Replace(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(", ");
            emitExpr(es, *e.args[1], 0);
            es.w(", 1)");
            return;
        }
        if (e.name == "matches") {
            es.ctx.needRegex = true;
            es.w("Regex.IsMatch(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
        if (e.name == "startsWith") { es.w(recv + ".StartsWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "endsWith") { es.w(recv + ".EndsWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Length == 0)"); return; }
        if (e.name == "isBlank") { es.w("string.IsNullOrWhiteSpace(" + recv + ")"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "compareToIgnoreCase") {
            es.w("string.Compare(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ", true)");
            return;
        }
        if (e.name == "split") {
            es.ctx.needRegex = true;
            es.w("Regex.Split(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
        if (e.name == "toCharArray") { es.w(recv + ".ToCharArray()"); return; }
        if (e.name == "hashCode") { es.w(recv + ".GetHashCode()"); return; }
        if (e.name == "getClass") { es.w(recv + ".GetType()"); return; }
        if (e.name == "toString") { es.w(recv); return; }
        if (e.name == "concat") {
            es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")");
            return;
        }
        if (e.name == "repeat") {
            es.ctx.needLinq = true;
            es.w("string.Concat(Enumerable.Repeat(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w("))");
            return;
        }
        if (e.name == "intern") { es.w(std::string("string.Intern(") + recv + ")"); return; }
        if (e.name == "chars") { es.w(recv); return; }
        if (e.name == "bytes" || e.name == "getBytes") { es.ctx.needText = true; es.w("Encoding.UTF8.GetBytes(" + recv + ")"); return; }
        if (e.name == "codePointAt") {
            es.w("(int)" + recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
    }
    if (rt.dims > 0) {
        if (e.name == "length") { es.w(recv + ".Length"); return; }
        if (e.name == "clone") {
            es.w("(" + csTypeStr(es.ctx, e.ty) + ")" + recv + ".Clone()");
            return;
        }
    }
    if (rname == "List" || rname == "IList" || rname == "LinkedList" || rname == "ICollection") {
        if (e.name == "size") { es.w(recv + ".Count"); return; }
        if (e.name == "get" && rname != "ICollection") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "set" && rname != "ICollection") {
            es.w("(" + recv + "[" + captureExprText(es, *e.args[0], 0) + "] = " +
                 captureExprText(es, *e.args[1], 0) + ")");
            return;
        }
        if (e.name == "add") {
            if (e.args.size() == 2 && rname != "ICollection") {
                es.w(recv + ".Insert(" + captureExprText(es, *e.args[0], 0) + ", " +
                     captureExprText(es, *e.args[1], 0) + ")");
            } else if (rname == "LinkedList") {
                es.w(recv + ".AddLast(");
                emitArgs(es, e.args);
                es.w(")");
            } else {
                es.w(recv + ".Add(");
                emitArgs(es, e.args);
                es.w(")");
            }
            return;
        }
        if (e.name == "remove") {
            if (e.args.size() == 1) {
                TypeInfo at = inferExprType(es, *e.args[0]);
                bool idx = at.known && (at.name == "int" || at.name == "long" || at.name == "short" ||
                                        at.name == "byte" || at.name == "char");
                if (!at.known && e.args[0]->k == EK::Lit && e.args[0]->lt == TT::Int) idx = true;
                if (rname != "ICollection") {
                    if (idx) es.w(recv + ".RemoveAt(" + captureExprText(es, *e.args[0], 0) + ")");
                    else es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ")");
                } else es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ")");
                return;
            }
        }
        if (e.name == "contains") { es.w(recv + ".Contains("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "indexOf") { es.w(recv + ".IndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "lastIndexOf") { es.w(recv + ".LastIndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "clear") { es.w(recv + ".Clear()"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "addAll" && rname != "ICollection") { es.w(recv + ".AddRange("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "removeAll" && rname != "ICollection") {
            es.w(recv + ".RemoveAll(x => " + captureExprText(es, *e.args[0], P_PRIMARY) +
                 ".Contains(x))");
            es.ctx.needLinq = true;
            return;
        }
        if (e.name == "removeIf" && rname != "ICollection") { es.w(recv + ".RemoveAll("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "sort") { es.w(recv + ".Sort("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "forEach" && rname != "ICollection") { es.w(recv + ".ForEach("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "iterator") { es.w(recv + ".GetEnumerator()"); return; }
        if (e.name == "toArray" && rname != "ICollection") { es.w(recv + ".ToArray()"); return; }
        if (e.name == "subList" && rname == "List") {
            es.w(recv + ".GetRange(" + captureExprText(es, *e.args[0], 0) + ", " +
                 captureExprText(es, *e.args[1], 0) + " - " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "getFirst") {
            es.ctx.needLinq = true;
            es.w(rname == "LinkedList" ? recv + ".First.Value" : recv + ".First()");
            return;
        }
        if (e.name == "peek" || e.name == "element") {
            es.ctx.needLinq = true;
            es.w(rname == "LinkedList" ? recv + ".First.Value" : recv + ".First()");
            return;
        }
        if (e.name == "getLast" || e.name == "peekLast") {
            es.ctx.needLinq = true;
            es.w(rname == "LinkedList" ? recv + ".Last.Value" : recv + ".Last()");
            return;
        }
        if (e.name == "poll" || e.name == "pollFirst" ||
            ((e.name == "remove") && e.args.empty())) {
            es.ctx.needLinq = true;
            es.ctx.needCollectionsSpec = true;
            if (rname == "LinkedList") {
                es.w("__JHelp.Poll(" + recv + ")");
            } else if (rname == "List" || rname == "IList") {
                es.w("__JHelp.Poll(" + recv + ")");
            } else es.w(recv + ".FirstOrDefault()");
            return;
        }
        if (e.name == "pollLast") {
            es.ctx.needCollectionsSpec = true;
            es.w("__JHelp.PollLast(" + recv + ")");
            return;
        }
        if (e.name == "addFirst") {
            if (rname == "LinkedList") es.w(recv + ".AddFirst(");
            else es.w(recv + ".Insert(0, ");
            emitArgs(es, e.args);
            es.w(rname == "LinkedList" ? ")" : ")");
            return;
        }
        if (e.name == "addLast") {
            if (rname == "LinkedList") es.w(recv + ".AddLast(");
            else es.w(recv + ".Add(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "removeFirst") {
            if (rname == "LinkedList") es.w(recv + ".RemoveFirst()");
            else es.w(recv + ".RemoveAt(0)");
            return;
        }
        if (e.name == "removeLast") {
            if (rname == "LinkedList") es.w(recv + ".RemoveLast()");
            else es.w(recv + ".RemoveAt(" + recv + ".Count - 1)");
            return;
        }
        if (e.name == "push") {
            if (rname == "LinkedList") es.w(recv + ".AddFirst(");
            else es.w(recv + ".Insert(0, ");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "pop") {
            es.ctx.needCollectionsSpec = true;
            if (rname == "LinkedList") es.w("__JHelp.PollLast(" + recv + ")");
            else es.w("__JHelp.Poll(" + recv + ")");
            return;
        }
        if (e.name == "offer") {
            if (rname == "LinkedList") es.w(recv + ".AddLast(");
            else es.w(recv + ".Add(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
    }
    if (rname == "HashSet" || rname == "SortedSet" || rname == "ISet") {
        if (e.name == "add") { es.w(recv + ".Add("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "contains") { es.w(recv + ".Contains("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "remove") { es.w(recv + ".Remove("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "size") { es.w(recv + ".Count"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "addAll") { es.w(recv + ".UnionWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "removeAll") { es.w(recv + ".ExceptWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "retainAll") { es.w(recv + ".IntersectWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "clear") { es.w(recv + ".Clear()"); return; }
        if (e.name == "iterator") { es.w(recv + ".GetEnumerator()"); return; }
        if (e.name == "toArray") { es.w(recv + ".ToArray()"); return; }
    }
    if (rname == "Dictionary" || rname == "SortedDictionary") {
        if (e.name == "put") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], P_PRIMARY) + "] = " +
                 captureExprText(es, *e.args[1], 0));
            return;
        }
        if (e.name == "get") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "getOrDefault") {
            std::string tmp = "__g" + std::to_string(es.tmpCounter++);
            es.w("(" + recv + ".TryGetValue(" + captureExprText(es, *e.args[0], 0) + ", out var " +
                 tmp + ") ? " + tmp + " : " + captureExprText(es, *e.args[1], 0) + ")");
            return;
        }
        if (e.name == "containsKey") { es.w(recv + ".ContainsKey("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "containsValue") { es.w(recv + ".ContainsValue("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "remove") { es.w(recv + ".Remove("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "size") { es.w(recv + ".Count"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "keySet") { es.w(recv + ".Keys"); return; }
        if (e.name == "values") { es.w(recv + ".Values"); return; }
        if (e.name == "clear") { es.w(recv + ".Clear()"); return; }
    }
    if (rname == "StringBuilder") {
        if (e.name == "append") { es.w(recv + ".Append("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "insert") { es.w(recv + ".Insert("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "length") { es.w(recv + ".Length"); return; }
        if (e.name == "charAt") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "setCharAt") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "] = " +
                 captureExprText(es, *e.args[1], 0));
            return;
        }
        if (e.name == "deleteCharAt") {
            es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ", 1)");
            return;
        }
        if (e.name == "delete") {
            es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ", " +
                 captureExprText(es, *e.args[1], 0) + " - " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "setLength") {
            es.w("(" + recv + ".Length = " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "reverse") {
            es.ctx.needLinq = true;
            es.w("new string(" + recv + ".ToString().Reverse().ToArray())");
            return;
        }
        if (e.name == "indexOf") {
            es.w(recv + ".ToString().IndexOf(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Length == 0)"); return; }
    }
    if (rname == "Stack") {
        if (e.name == "push") { es.w(recv + ".Push("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "pop") { es.w(recv + ".Pop()"); return; }
        if (e.name == "peek") { es.w(recv + ".Peek()"); return; }
        if (e.name == "empty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "size") { es.w(recv + ".Count"); return; }
    }
    if (es.ctx.simpleEnums.count(rname)) {
        if (e.name == "compareTo" && e.args.size() == 1) {
            es.w("(((int)" + recv + ").CompareTo((int)" + captureExprText(es, *e.args[0], P_PRIMARY) + "))");
            return;
        }
        if (e.name == "equals" && e.args.size() == 1) {
            es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_EQ) + ")");
            return;
        }
        if (e.name == "toString" && e.args.empty()) { es.w(recv + ".ToString()"); return; }
        if (e.name == "ordinal" && e.args.empty()) { es.w("(int)" + recv); return; }
        if (e.name == "name" && e.args.empty()) { es.w(recv + ".ToString()"); return; }
    }
    if (es.ctx.enumClassTypes.count(rname)) {
        if (e.name == "compareTo" && e.args.size() == 1) {
            es.w("(" + recv + ".Ordinal - " + captureExprText(es, *e.args[0], P_ADD) + ".Ordinal)");
            return;
        }
        if (e.name == "equals" && e.args.size() == 1) {
            es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_EQ) + ")");
            return;
        }
        if (e.name == "ordinal" && e.args.empty()) { es.w(recv + ".Ordinal"); return; }
        if (e.name == "name" && e.args.empty()) { es.w(recv + ".Name"); return; }
    }
    if (rname == "IEnumerator") {
        if (e.name == "hasNext") { es.w(recv + ".MoveNext()"); return; }
        if (e.name == "next") { es.w(recv + ".Current"); return; }
    }
    if (endsWith(rname, "?")) {
        if (e.name == "isPresent") { es.w("(" + recv + " != null)"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + " == null)"); return; }
        if (e.name == "orElse") {
            es.w("(" + recv + " ?? " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "orElseGet") {
            es.w("(" + recv + " ?? " + captureExprText(es, *e.args[0], 0) + "())");
            return;
        }
        if (e.name == "get") { es.w(recv + "!"); return; }
        if (e.name == "ofNullable" || e.name == "of") {
            if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
            return;
        }
    }
    if (rname == "IEnumerable" || rname == "List" || rname == "LinkedList" ||
        rname == "ICollection" || rname == "HashSet" || rname == "SortedSet") {
        static const std::map<std::string, std::string> linq1 = {
            {"filter", "Where"}, {"map", "Select"}, {"mapToInt", "Select"},
            {"mapToLong", "Select"}, {"mapToDouble", "Select"}, {"mapToObj", "Select"},
            {"flatMap", "SelectMany"}, {"distinct", "Distinct"}, {"sorted", "OrderBy"},
            {"limit", "Take"}, {"skip", "Skip"}, {"anyMatch", "Any"}, {"allMatch", "All"},
            {"noneMatch", "All"},
            {"findFirst", "FirstOrDefault"}, {"findAny", "FirstOrDefault"},
            {"count", "Count"}, {"sum", "Sum"}, {"average", "Average"}, {"max", "Max"},
            {"min", "Min"}, {"reduce", "Aggregate"}, {"forEach", "ToList().ForEach"}};
            if (e.name == "collect" && !e.args.empty() && e.args[0]->k == EK::Call) {
                Expr& coll = *e.args[0];
                std::string cp = pathText(*coll.obj);
                if (coll.name == "toList" && (cp == "Collectors" || cp == "java.util.stream.Collectors")) {
                    es.w(recv + ".ToList()");
                    return;
                }
                if (coll.name == "toSet" && (cp == "Collectors" || cp == "java.util.stream.Collectors")) {
                    es.w(recv + ".ToHashSet()");
                    return;
                }
                if (coll.name == "joining") {
                    es.w("string.Join(");
                    emitArgs(es, coll.args);
                    es.w(", " + recv + ")");
                    return;
                }
                if (coll.name == "toMap") {
                    es.w(recv + ".ToDictionary(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
                if (coll.name == "counting" && coll.args.empty()) {
                    es.w(recv + ".Count()");
                    return;
                }
                if (coll.name == "summingInt" || coll.name == "summingLong" ||
                    coll.name == "summingDouble") {
                    es.w(recv + ".Sum(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
                if (coll.name == "averagingDouble" || coll.name == "averagingInt") {
                    es.w(recv + ".Average(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
                if (coll.name == "groupingBy") {
                    es.w(recv + ".ToLookup(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
            }
            if (e.name == "collect") {
                es.w(recv + ".ToList()");
                return;
            }
        auto it = linq1.find(e.name);
        if (it != linq1.end()) {
            std::string m = it->second;
            if (e.name == "forEach") {
                if (rname == "List" || rname == "LinkedList") m = "ForEach";
                else {
                    es.ctx.needLinq = true;
                    m = "ToList().ForEach";
                }
            }
            if (e.name == "forEach") es.ctx.needLinq = true;
            if (e.name == "sorted" && e.args.size() == 1) {
                TypeInfo et = elemTypeOf(rt);
                if (et.known) {
                    es.w(recv + ".OrderBy(x => x, Comparer<" + et.text() + ">.Create(");
                    emitExpr(es, *e.args[0], 0);
                    es.w("))");
                    return;
                }
            }
            if (e.name == "noneMatch" && e.args.size() == 1 && e.args[0]->k == EK::Lambda &&
                !e.args[0]->lbodyIsBlock) {
                es.ctx.needLinq = true;
                es.w(recv + ".All(");
                Expr& lam = *e.args[0];
                if (lam.lparams.size() == 1) es.w(lam.lparams[0].second);
                else es.w("x");
                es.w(" => !(");
                emitExpr(es, *lam.lbody, 0);
                es.w("))");
                return;
            }
            if (e.name == "sorted" && e.args.empty()) {
                es.ctx.needLinq = true;
                es.w(recv + ".OrderBy(x => x)");
                return;
            }
            es.ctx.needLinq = true;
            es.w(recv + "." + m + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "size") { es.w(recv + ".Count()"); es.ctx.needLinq = true; return; }
        if (e.name == "isEmpty") { es.w("(!" + recv + ".Any())"); es.ctx.needLinq = true; return; }
        if (e.name == "toArray") { es.w(recv + ".ToArray()"); es.ctx.needLinq = true; return; }
        if (e.name == "iterator") { es.w(recv + ".GetEnumerator()"); return; }
    }
    if (rname == "Thread") {
        if (e.name == "start") { es.w(recv + ".Start()"); return; }
        if (e.name == "join") { es.w(recv + ".Join("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "getName") { es.w(recv + ".Name"); return; }
        if (e.name == "setName") {
            es.w("(" + recv + ".Name = " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "interrupt") { es.w(recv + ".Interrupt()"); return; }
        if (e.name == "isAlive") { es.w(recv + ".IsAlive"); return; }
    }
    if (rname == "Random") {
        if (e.name == "nextInt") {
            if (e.args.empty()) es.w(recv + ".Next()");
            else { es.w(recv + ".Next("); emitArgs(es, e.args); es.w(")"); }
            return;
        }
        if (e.name == "nextLong") { es.w(recv + ".NextInt64()"); return; }
        if (e.name == "nextDouble") { es.w(recv + ".NextDouble()"); return; }
        if (e.name == "nextBoolean") { es.w("(" + recv + ".Next(2) == 0)"); return; }
        if (e.name == "nextGaussian") { es.w(recv + ".NextDouble()"); return; }
    }
    if (rname == "Scanner") {
        if (e.name == "nextLine") { es.w("Console.ReadLine()"); return; }
        if (e.name == "nextInt") { es.w("int.Parse(Console.ReadLine())"); return; }
        if (e.name == "nextLong") { es.w("long.Parse(Console.ReadLine())"); return; }
        if (e.name == "nextDouble") { es.w("double.Parse(Console.ReadLine())"); return; }
        if (e.name == "nextBoolean") { es.w("bool.Parse(Console.ReadLine())"); return; }
        if (e.name == "next") { es.w("Console.ReadLine()"); return; }
        if (e.name == "hasNextLine" || e.name == "hasNext") {
            es.w("(Console.In.Peek() != -1)");
            return;
        }
        if (e.name == "close") { es.w(recv); return; }
    }
    if (rname == "string" || rname == "StreamReader" || rname == "StringReader" ||
        rname == "StreamWriter" || rname == "StringWriter") {
        if ((rname == "StreamReader" || rname == "StringReader") && e.name == "readLine") { es.w(recv + ".ReadLine()"); return; }
        if ((rname == "StreamReader" || rname == "StringReader") && e.name == "read") { es.w(recv + ".Read()"); return; }
        if (rname == "StreamWriter") {
            if (e.name == "write") { es.w(recv + ".Write("); emitArgs(es, e.args); es.w(")"); return; }
            if (e.name == "println") { es.w(recv + ".WriteLine("); emitArgs(es, e.args); es.w(")"); return; }
            if (e.name == "flush") { es.w(recv + ".Flush()"); return; }
            if (e.name == "close") { es.w(recv + ".Close()"); return; }
        }
    }
    if (rname == "Exception" || endsWith(rname, "Exception")) {
        bool userOwns =
            es.ctx.instanceMethods.count(rname + "." + e.name + "/" + std::to_string(e.args.size())) ||
            es.ctx.staticMethods.count(rname + "." + e.name + "/" + std::to_string(e.args.size()));
        if (!userOwns) {
            if (e.name == "getMessage" || e.name == "getLocalizedMessage") { es.w(recv + ".Message"); return; }
            if (e.name == "printStackTrace") {
                es.w("Console.Error.WriteLine(" + recv + ")");
                return;
            }
            if (e.name == "getCause") { es.w(recv + ".InnerException"); return; }
            if (e.name == "getStackTrace") { es.w(recv + ".StackTrace"); return; }
            if (e.name == "getClass") { es.w(recv + ".GetType()"); return; }
            if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        }
    }
    if (rname == "Guid") {
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
    }
    if (rname == "BigInteger") {
        if (e.name == "add") { es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "subtract") { es.w("(" + recv + " - " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "multiply") { es.w("(" + recv + " * " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "divide") { es.w("(" + recv + " / " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "mod") { es.w("(" + recv + " % " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "negate") { es.w("(-" + recv + ")"); return; }
        if (e.name == "abs") { es.w("BigInteger.Abs(" + recv + ")"); return; }
        if (e.name == "pow") { es.w("BigInteger.Pow(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "gcd") { es.w("BigInteger.GreatestCommonDivisor(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "equals") { es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "intValue") { es.w("((int)" + recv + ")"); return; }
        if (e.name == "longValue") { es.w("((long)" + recv + ")"); return; }
        if (e.name == "doubleValue") { es.w("((double)" + recv + ")"); return; }
        if (e.name == "intValueExact") { es.w("((int)" + recv + ")"); return; }
        if (e.name == "longValueExact") { es.w("((long)" + recv + ")"); return; }
        if (e.name == "sign") { es.w(recv + ".Sign"); return; }
        if (e.name == "shiftLeft") { es.w("(" + recv + " << " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "shiftRight") { es.w("(" + recv + " >> " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "and") { es.w("(" + recv + " & " + captureExprText(es, *e.args[0], P_BITAND) + ")"); return; }
        if (e.name == "or") { es.w("(" + recv + " | " + captureExprText(es, *e.args[0], P_BITOR) + ")"); return; }
        if (e.name == "xor") { es.w("(" + recv + " ^ " + captureExprText(es, *e.args[0], P_XOR) + ")"); return; }
        if (e.name == "not") { es.w("(~" + recv + ")"); return; }
    }
    if (rname == "decimal") {
        if (e.name == "add") { es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "subtract") { es.w("(" + recv + " - " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "multiply") { es.w("(" + recv + " * " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "divide") { es.w("(" + recv + " / " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "equals") { es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "intValue") { es.w("((int)" + recv + ")"); return; }
        if (e.name == "longValue") { es.w("((long)" + recv + ")"); return; }
        if (e.name == "doubleValue") { es.w("((double)" + recv + ")"); return; }
        if (e.name == "setScale") {
            if (e.args.size() == 2) {
                es.w("Math.Round(" + recv + ", " + captureExprText(es, *e.args[0], 0) +
                     ", MidpointRounding.AwayFromZero)");
                return;
            }
            es.w("Math.Round(" + recv + ", " + captureExprText(es, *e.args[0], 0) +
                 ", MidpointRounding.AwayFromZero)");
            return;
        }
        if (e.name == "abs") { es.w("Math.Abs(" + recv + ")"); return; }
        if (e.name == "signum") { es.w("Math.Sign(" + recv + ")"); return; }
        if (e.name == "max") { es.w("Math.Max(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "min") { es.w("Math.Min(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
    }
    if (rname == "DateTime" || rname == "DateOnly" || rname == "TimeOnly" ||
        rname == "DateTimeOffset") {
        if (e.name == "plusDays") { es.w(recv + ".AddDays("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusWeeks") { es.w(recv + ".AddDays("); emitExpr(es, *e.args[0], 0); es.w(" * 7)"); return; }
        if (e.name == "plusMonths") { es.w(recv + ".AddMonths("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusYears") { es.w(recv + ".AddYears("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusHours") { es.w(recv + ".AddHours("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusMinutes") { es.w(recv + ".AddMinutes("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusSeconds") { es.w(recv + ".AddSeconds("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusNanos") { es.w(recv + ".AddMilliseconds("); emitExpr(es, *e.args[0], 0); es.w(" / 1e6)"); return; }
        if (e.name == "minusDays") { es.w(recv + ".AddDays(-("); emitArgs(es, e.args); es.w("))"); return; }
        if (e.name == "minusMonths") { es.w(recv + ".AddMonths(-("); emitArgs(es, e.args); es.w("))"); return; }
        if (e.name == "minusYears") { es.w(recv + ".AddYears(-("); emitArgs(es, e.args); es.w("))"); return; }
        if (e.name == "getYear") { es.w(recv + ".Year"); return; }
        if (e.name == "getMonthValue") { es.w(recv + ".Month"); return; }
        if (e.name == "getDayOfMonth") { es.w(recv + ".Day"); return; }
        if (e.name == "getDayOfWeek") { es.w(recv + ".DayOfWeek"); return; }
        if (e.name == "getHour") { es.w(recv + ".Hour"); return; }
        if (e.name == "getMinute") { es.w(recv + ".Minute"); return; }
        if (e.name == "getSecond") { es.w(recv + ".Second"); return; }
        if (e.name == "isBefore") { es.w("(" + recv + " < " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "isAfter") { es.w("(" + recv + " > " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "isEqual") { es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_EQ) + ")"); return; }
        if (e.name == "format") {
            if (!e.args.empty() && isStringLit(*e.args[0])) {
                es.w(recv + ".ToString(\"" + escapeCsStr(translateDatePattern(e.args[0]->val)) + "\")");
            } else {
                es.w(recv + ".ToString(");
                emitArgs(es, e.args);
                es.w(")");
            }
            return;
        }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "toEpochMilli") { es.w(recv + ".ToUnixTimeMilliseconds()"); return; }
        if (e.name == "getEpochSecond") { es.w(recv + ".ToUnixTimeSeconds()"); return; }
    }
    if (rname == "TimeSpan") {
        if (e.name == "toMillis") { es.w("((long)" + recv + ".TotalMilliseconds)"); return; }
        if (e.name == "toSeconds") { es.w("((long)" + recv + ".TotalSeconds)"); return; }
        if (e.name == "getSeconds") { es.w("((long)" + recv + ".TotalSeconds)"); return; }
        if (e.name == "getMinutes") { es.w(recv + ".Minutes"); return; }
        if (e.name == "getHours") { es.w(recv + ".Hours"); return; }
        if (e.name == "getDays") { es.w(recv + ".Days"); return; }
        if (e.name == "isZero") { es.w("(" + recv + " == TimeSpan.Zero)"); return; }
        if (e.name == "isNegative") { es.w("(" + recv + " < TimeSpan.Zero)"); return; }
        if (e.name == "abs") { es.w(recv + ".Duration()"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "multipliedBy") { es.w("(" + recv + " * " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "dividedBy") { es.w("(" + recv + " / " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "plus") { es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "minus") { es.w("(" + recv + " - " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
    }
    if (rname == "KeyValuePair") {
        if (e.name == "getKey") { es.w(recv + ".Key"); return; }
        if (e.name == "getValue") { es.w(recv + ".Value"); return; }
    }
    static const std::set<std::string> samCallNames = {
        "run", "get", "call", "apply", "accept", "test", "compare",
        "getAsInt", "getAsLong", "getAsDouble", "getAsBoolean"};
    bool isDelegateType = rname == "Action" || startsWith(rname, "Func") ||
                          rname == "Predicate" || startsWith(rname, "Predicate<") ||
                          startsWith(rname, "Comparison");
    if (isDelegateType && samCallNames.count(e.name)) {
        es.w(recv + ".Invoke(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    if (rname == "Match") {
        if (e.name == "group") {
            if (e.args.empty()) es.w(recv + ".Value");
            else {
                TypeInfo at = inferExprType(es, *e.args[0]);
                if (at.known && at.name == "string")
                    es.w(recv + ".Groups[" + captureExprText(es, *e.args[0], 0) + "].Value");
                else es.w(recv + ".Groups[" + captureExprText(es, *e.args[0], 0) + "].Value");
            }
            return;
        }
        if (e.name == "start") { es.w(recv + ".Index"); return; }
        if (e.name == "end") { es.w("(" + recv + ".Index + " + recv + ".Length)"); return; }
        if (e.name == "matches" || e.name == "find") { es.w(recv + ".Success"); return; }
    }
    if (rname == "Regex") {
        if (e.name == "matcher") { es.w(recv + ".Match("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "split") { es.w(recv + ".Split("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "pattern") { es.w(recv + ".ToString()"); return; }
    }
    if (rname == "StreamReader" || rname == "StreamWriter" || rname == "TextWriter") {
        if (e.name == "close") { es.w(recv + ".Close()"); return; }
        if (e.name == "flush") { es.w(recv + ".Flush()"); return; }
    }
    if (e.name == "getAnnotation" && e.args.size() == 1 && e.args[0]->k == EK::ClassLit) {
        std::string an = csTypeStr(es.ctx, e.args[0]->ty, true);
        es.ctx.needReflection = true;
        es.w("((" + an + ")" + recv + ".GetCustomAttribute(typeof(" + an + ")))");
        return;
    }
    if (e.name == "compareTo" && e.args.size() == 1) {
        es.w(recv + ".CompareTo(" + captureExprText(es, *e.args[0], P_ADD) + ")");
        return;
    }
    if (e.name == "equals" && e.args.size() == 1) {
        es.w(recv + ".Equals(" + captureExprText(es, *e.args[0], P_ADD) + ")");
        return;
    }
    if (e.name == "hashCode" && e.args.empty()) {
        es.w(recv + ".GetHashCode()");
        return;
    }
    if (e.name == "toString" && e.args.empty()) {
        es.w(recv + ".ToString()");
        return;
    }
    if (e.name == "getClass" && e.args.empty()) {
        es.w(recv + ".GetType()");
        return;
    }
    if (!rt.known) {
        if (e.name == "length" && e.args.empty()) { es.w(recv + ".Length"); return; }
        if (e.name == "size" && e.args.empty()) { es.ctx.needLinq = true; es.w(recv + ".Count()"); return; }
        if (e.name == "isEmpty" && e.args.empty()) { es.ctx.needLinq = true; es.w("(!" + recv + ".Any())"); return; }
        if ((e.name == "intValue" || e.name == "intValueExact") && e.args.empty()) { es.w("((int)" + recv + ")"); return; }
        if ((e.name == "longValue" || e.name == "longValueExact") && e.args.empty()) { es.w("((long)" + recv + ")"); return; }
        if (e.name == "doubleValue" && e.args.empty()) { es.w("((double)" + recv + ")"); return; }
        if (e.name == "floatValue" && e.args.empty()) { es.w("((float)" + recv + ")"); return; }
        if (e.name == "contains" && e.args.size() == 1) { es.w(recv + ".Contains(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "startsWith" && e.args.size() == 1) { es.w(recv + ".StartsWith(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "endsWith" && e.args.size() == 1) { es.w(recv + ".EndsWith(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "toUpperCase" && e.args.empty()) { es.w(recv + ".ToUpper()"); return; }
        if (e.name == "toLowerCase" && e.args.empty()) { es.w(recv + ".ToLower()"); return; }
        if (e.name == "trim" && e.args.empty()) { es.w(recv + ".Trim()"); return; }
        if (e.name == "strip" && e.args.empty()) { es.w(recv + ".Trim()"); return; }
        if (e.name == "toString" && e.args.empty()) { es.w(recv + ".ToString()"); return; }
        if (e.name == "hashCode" && e.args.empty()) { es.w(recv + ".GetHashCode()"); return; }
        if (e.name == "getClass" && e.args.empty()) { es.w(recv + ".GetType()"); return; }
        if (e.name == "isEmpty" && e.args.empty()) { es.w(recv + ".Length == 0"); return; }
        if (e.name == "compareTo" && e.args.size() == 1) { es.w(recv + ".CompareTo(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "equals" && e.args.size() == 1) { es.w(recv + ".Equals(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
    }
    if (rt.known && es.ctx.samDelegates.count(rt.name) &&
        es.ctx.samMethodNames[rt.name] == e.name) {
        es.w(recv + ".Invoke(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    es.w(recv + "." + e.name + "(");
    emitArgs(es, e.args);
    es.w(")");
}

void emitExpr(ES& es, Expr& e, int prec, const TypeInfo& expect) {
    switch (e.k) {
        case EK::Lit:
            emitLiteral(es, e, expect);
            break;
        case EK::Ident:
            emitIdent(es, e);
            break;
        case EK::This:
            es.w("this");
            break;
        case EK::Field:
            emitFieldExpr(es, e);
            break;
        case EK::Call:
            emitCall(es, e, expect);
            break;
        case EK::New:
            emitNewExpr(es, e, expect);
            break;
        case EK::NewArr:
            emitNewArrExpr(es, e, expect);
            break;
        case EK::ArrIdx:
            if (e.a) emitExpr(es, *e.a, P_POST);
            es.w("[");
            if (e.b) emitExpr(es, *e.b, 0);
            es.w("]");
            break;
        case EK::Bin:
            emitBinExpr(es, e, prec);
            break;
        case EK::Un:
            emitUnExpr(es, e, prec);
            break;
        case EK::Post:
            emitPostExpr(es, e, prec);
            break;
        case EK::Cast:
            emitCastExpr(es, e, prec, expect);
            break;
        case EK::Assign:
            emitAssignExpr(es, e, prec);
            break;
        case EK::Ternary:
            emitTernaryExpr(es, e, prec);
            break;
        case EK::Lambda:
            emitLambdaExpr(es, e);
            break;
        case EK::MRef:
            emitMRefExpr(es, e, expect);
            break;
        case EK::InstOf:
            emitInstOfExpr(es, e, prec);
            break;
        case EK::SwitchE:
            emitSwitchEExpr(es, e, expect);
            break;
        case EK::ClassLit:
            emitClassLitExpr(es, e);
            break;
        case EK::CtorCall:
            emitCtorCallExpr(es, e);
            break;
        case EK::Paren:
        case EK::TypeE:
        default:
            if (e.a) emitExpr(es, *e.a, prec);
            break;
    }
}
}

namespace js {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    if (f.bad()) return false;
    out = ss.str();
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB &&
        (unsigned char)out[2] == 0xBF)
        out = out.substr(3);
    return true;
}

bool writeFile(const std::string& path, const std::string& content) {
    std::filesystem::path p(path);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    f.flush();
    if (!f) return false;
    f.close();
    return static_cast<bool>(f);
}

void printUsage() {
    std::cout << PROGNAME << " " << VERSION << " - Java to C# converter\n";
    std::cout << "\n";
    std::cout << "Usage:\n";
    std::cout << "  javasharp --convert <input.java> to <output.cs> [--convert <input.java> to <output.cs> ...]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  --convert <in.java> to <out.cs>   Convert a Java source file to C#\n";
    std::cout << "                                    (the pair may be repeated)\n";
    std::cout << "  --version                         Print version information\n";
    std::cout << "  --help                            Print this help\n";
    std::cout << "\n";
    std::cout << "Exit codes:\n";
    std::cout << "  0  conversion succeeded\n";
    std::cout << "  1  conversion completed with syntax errors in the input\n";
    std::cout << "  2  usage error or input/output file error\n";
}

int convertOne(const std::string& inPath, const std::string& outPath) {
    std::string src;
    if (!readFile(inPath, src)) {
        std::cerr << PROGNAME << ": error: cannot read input file '" << inPath << "'\n";
        return 2;
    }
    Ctx ctx;
    Lex lex(src, ctx);
    lex.run();
    Parser parser(std::move(lex.toks), ctx);
    parser.ctx.comments = lex.comments;
    auto unit = parser.parseUnit();
    for (auto& e : ctx.errors) std::cerr << PROGNAME << ": " << inPath << " " << e << "\n";
    if (!unit) return 2;
    processImports(ctx, *unit);
    for (auto& d : unit->types) {
        registerDecl(ctx, *d);
        collectClassInfo(ctx, *d);
    }
    detectSamDelegates(ctx, *unit);
    hoistUnit(ctx, *unit);
    for (auto& d : unit->types) {
        registerDecl(ctx, *d);
        collectClassInfo(ctx, *d);
    }
    resolveOverrides(ctx, *unit);
    classifyTypes(ctx, *unit);
    detectSamDelegates(ctx, *unit);
    countMains(ctx, *unit);
    for (auto& d : unit->types) {
        for (auto& m : d->members)
            if (m.mk == 4 && m.nested) {
                registerDecl(ctx, *m.nested);
                collectClassInfo(ctx, *m.nested);
            }
    }
    std::string result = convertUnit(ctx, *unit);
    if (!writeFile(outPath, result)) {
        std::cerr << PROGNAME << ": error: cannot write output file '" << outPath << "'\n";
        return 2;
    }
    size_t lines = 0;
    for (char c : result)
        if (c == '\n') lines++;
    std::cout << PROGNAME << ": converted " << inPath << " -> " << outPath << " (" << lines
              << " lines";
    if (!ctx.errors.empty()) std::cout << ", " << ctx.errors.size() << " syntax errors";
    std::cout << ")\n";
    return ctx.errors.empty() ? 0 : 1;
}
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        js::printUsage();
        return args.size() == 1 ? 0 : 2;
    }
    if (!args.empty() && (args[0] == "--version" || args[0] == "-v")) {
        if (args.size() != 1) {
            std::cerr << js::PROGNAME << ": error: --version does not accept additional arguments\n";
            return 2;
        }
        std::cout << js::PROGNAME << " " << js::VERSION << "\n";
        return 0;
    }
    if (args.empty() || args[0] != "--convert") {
        std::cerr << js::PROGNAME << ": error: expected --convert\n\n";
        js::printUsage();
        return 2;
    }
    size_t i = 0;
    int converted = 0, failed = 0, partial = 0;
    while (i < args.size()) {
        if (args[i] == "--convert") i++;
        if (i >= args.size()) {
            std::cerr << js::PROGNAME << ": error: incomplete --convert command\n";
            return 2;
        }
        if (i + 2 >= args.size()) {
            std::cerr << js::PROGNAME << ": error: incomplete conversion pair near '" << args[i] << "'\n";
            std::cerr << "Expected: javasharp --convert <input.java> to <output.cs>\n";
            return 2;
        }
        const std::string& inFile = args[i];
        const std::string& toKw = args[i + 1];
        const std::string& outFile = args[i + 2];
        if (toKw != "to") {
            std::cerr << js::PROGNAME << ": error: expected 'to' between input and output, got '"
                      << toKw << "'\n";
            return 2;
        }
        int rc = js::convertOne(inFile, outFile);
        if (rc == 0) converted++;
        else if (rc == 1) partial++;
        else failed++;
        i += 3;
        if (i < args.size() && args[i] != "--convert" && i + 2 >= args.size()) {
            std::cerr << js::PROGNAME << ": error: unexpected argument '" << args[i] << "'\n";
            return 2;
        }
    }
    if (converted + partial + failed == 0) return 2;
    return failed > 0 ? 2 : (partial > 0 ? 1 : 0);
}
