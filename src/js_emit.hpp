#pragma once
#include "js_semantic.hpp"
namespace js {
struct ES {
    Ctx& ctx;
    std::ostringstream out;
    std::ostream* cur;
    int indent = 0;
    std::string cls;
    Decl* decl = nullptr;
    std::vector<VarInfo> scope;
    std::set<std::string> activeLabels;
    std::set<std::string> typeParams;
    int tmpCounter = 0;
    bool inCtor = false;
    bool isInterfaceCtx = false;
    std::string switchSubjectType;
    TypeRef curRet;
    bool hasCurRet = false;

    explicit ES(Ctx& c) : ctx(c), cur(&out) {}

    void w(const std::string& s) { (*cur) << s; }
    void wi() {
        for (int i = 0; i < indent; i++) (*cur) << "    ";
    }
    void nl() {
        (*cur) << "\n";
    }
    void line(const std::string& s) {
        wi();
        w(s);
        nl();
    }
};

void emitStmt(ES& es, Stmt& st);
void emitExpr(ES& es, Expr& e, int prec = 0, const TypeInfo& expect = TypeInfo());
TypeInfo inferExprType(ES& es, Expr& e);
void flushComments(ES& es, int line);
void flushTrailing(ES& es, int line);
void emitType(ES& es, Decl& d);
void emitBodyBlock(ES& es, Stmt& body);
std::string delegateSigFor(Ctx& ctx, Decl& iface);
std::string captureExprText(ES& es, Expr& e, int prec = 0, const TypeInfo& expect = TypeInfo());

std::string pascalCase(const std::string& s);

void emitAnnotations(ES& es, std::vector<Ann>& anns) {
    for (auto& a : anns) {
        if (a.name == "Override" || a.name == "java.lang.Override" || a.name == "SuppressWarnings" ||
            a.name == "java.lang.SuppressWarnings" || a.name == "FunctionalInterface" ||
            a.name == "SafeVarargs" || a.name == "java.lang.SafeVarargs" ||
            a.name == "java.lang.FunctionalInterface")
            continue;
        flushComments(es, a.line);
        std::string mapped = a.name;
        size_t dot = mapped.rfind('.');
        if (dot != std::string::npos) {
            std::string simple = mapped.substr(dot + 1);
            if (startsWith(mapped, "java.lang.annotation") || startsWith(mapped, "java.lang.annotation.")) continue;
            if (startsWith(mapped, "java.lang")) mapped = simple;
            else if (startsWith(mapped, "java.")) continue;
        }
        if (mapped == "Deprecated") {
            es.wi();
            es.line("[Obsolete]");
            continue;
        }
        std::string s = "[" + mapped + "(";
        bool first = true;
        for (auto& pa : a.pargs) {
            if (!first) s += ", ";
            first = false;
            s += captureExprText(es, *pa);
        }
        for (auto& kv : a.args) {
            if (!first) s += ", ";
            first = false;
            s += pascalCase(kv.first) + " = " + captureExprText(es, *kv.second);
        }
        es.wi();
        es.w(s);
        es.w(")]");
        es.nl();
    }
}

std::string pascalCase(const std::string& s) {
    if (s.empty()) return s;
    std::string r = s;
    r[0] = (char)std::toupper((unsigned char)r[0]);
    return r;
}

std::string csTypeStr(Ctx& ctx, const TypeRef& t, bool genArg = false) {
    if (t.isVar) return "var";
    if (t.wildcard) {
        if (t.name.empty()) return "object";
        TypeRef b;
        b.name = t.name;
        b.args = t.args;
        return csTypeStr(ctx, b, genArg);
    }
    if (t.name.empty()) return "object";
    if (t.name == "Optional" && t.args.size() == 1) {
        return csTypeStr(ctx, t.args[0], true) + "?";
    }
    std::string mapped = mapJavaTypeFqn(t.name, genArg);
    if (mapped.empty()) mapped = t.name;
    if (mapped == "__Scanner") ctx.needScanner = true;
    if (mapped == "StringBuilder") ctx.needText = true;
    if (mapped == "Thread") ctx.needThreading = true;
    std::string r = mapped;
    if (!t.args.empty()) {
        r += "<";
        for (size_t i = 0; i < t.args.size(); i++) {
            if (i) r += ", ";
            r += csTypeStr(ctx, t.args[i], true);
        }
        r += ">";
    }
    for (int i = 0; i < t.dims; i++) r += "[]";
    return r;
}

std::string visOf(const std::vector<std::string>& mods) {
    if (hasMod(mods, "public")) return "public";
    if (hasMod(mods, "private")) return "private";
    if (hasMod(mods, "protected")) return "protected";
    return "internal";
}

std::string tparamsStr(ES& es, const std::vector<TP>& tps, bool* hasWhere) {
    std::string r;
    std::string where;
    if (!tps.empty()) {
        r += "<";
        for (size_t i = 0; i < tps.size(); i++) {
            if (i) r += ", ";
            r += tps[i].name;
        }
        r += ">";
        for (auto& tp : tps) {
            if (tp.hasBound && !tp.bound.empty()) {
                where += " where " + tp.name + " : " + csTypeStr(es.ctx, tp.bound);
            }
        }
    }
    if (hasWhere) *hasWhere = !where.empty();
    return r + where;
}

bool isConstAble(ES& es, Member& m) {
    std::string t = csTypeStr(es.ctx, m.ty);
    if (t != "int" && t != "long" && t != "double" && t != "float" && t != "bool" &&
        t != "string" && t != "char" && t != "byte" && t != "short" && t != "decimal")
        return false;
    for (auto& f : m.finits) {
        if (!f) return false;
        if (f->k != EK::Lit && !(f->k == EK::Un && f->a && f->a->k == EK::Lit)) return false;
    }
    return true;
}

void emitField(ES& es, Member& m) {
    flushComments(es, m.line);
    emitAnnotations(es, m.anns);
    std::string vis = visOf(m.mods);
    bool stat = hasMod(m.mods, "static");
    bool fin = hasMod(m.mods, "final");
    bool vol = hasMod(m.mods, "volatile");
    std::string base = csTypeStr(es.ctx, m.ty);
    bool constAble = fin && stat && isConstAble(es, m);
    for (size_t i = 0; i < m.names.size(); i++) {
        std::string t = base;
        for (int k = 0; k < m.fdims[i]; k++) t += "[]";
        std::string mods = vis + " ";
        if (constAble) mods += "const ";
        else {
            if (stat) mods += "static ";
            if (fin) mods += "readonly ";
        }
        if (vol && t != "long" && t != "double" && t != "float") mods += "volatile ";
        flushComments(es, m.line);
        es.wi();
        es.w(mods + t + " " + csIdent(m.names[i]));
        if (i < m.finits.size() && m.finits[i]) {
            es.w(" = ");
            TypeInfo expect = toInfo(es.ctx, m.ty);
            expect.dims += m.fdims[i];
            expect.known = !m.ty.name.empty();
            emitExpr(es, *m.finits[i], 0, expect);
        }
        es.w(";");
        flushTrailing(es, m.line);
        es.nl();
    }
}

void emitInitBlock(ES& es, Member& m, Decl& d) {
    if (m.isStaticInit) {
        flushComments(es, m.line);
        es.wi();
        es.w("static " + d.name + "()");
        es.nl();
        es.line("{");
        es.indent++;
        emitBodyBlock(es, *m.body);
        es.indent--;
        es.line("}");
    } else {
        es.wi();
        es.w("private void __InstanceInit()");
        es.nl();
        es.line("{");
        es.indent++;
        emitBodyBlock(es, *m.body);
        es.indent--;
        es.line("}");
        es.nl();
    }
}

std::string mapMethodName(ES& es, const std::string& recvType, const std::string& name) {
    (void)es;
    (void)recvType;
    return name;
}

std::string paramListStr(ES& es, Member& m) {
    std::string r;
    for (size_t i = 0; i < m.params.size(); i++) {
        if (i) r += ", ";
        if (m.varargs && i == m.params.size() - 1) {
            TypeRef at = m.params[i].first;
            if (at.dims == 0) at.dims = 1;
            r += "params " + csTypeStr(es.ctx, at) + " " + csIdent(m.params[i].second);
        } else {
            r += csTypeStr(es.ctx, m.params[i].first) + " " + csIdent(m.params[i].second);
        }
    }
    return r;
}

void emitMethodCommon(ES& es, Decl& d, Member& m, const std::string& name,
                      const std::string& modsPrefix, bool emitBody) {
    (void)d;
    flushComments(es, m.line);
    emitAnnotations(es, m.anns);
    bool hasWhere = false;
    std::string tp = tparamsStr(es, m.tparams, &hasWhere);
    std::string ret = csTypeStr(es.ctx, m.ty);
    std::set<std::string> savedTypeParams = es.typeParams;
    for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
    es.wi();
    std::string sig = modsPrefix;
    sig += ret + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")";
    es.w(sig);
    flushTrailing(es, m.line);
    es.nl();
    if (!emitBody || !m.body) {
        es.wi();
        es.w(";");
        es.nl();
        es.typeParams = savedTypeParams;
        return;
    }
    es.line("{");
    es.indent++;
    TypeRef savedRet = es.curRet;
    bool savedHas = es.hasCurRet;
    es.curRet = m.ty;
    es.hasCurRet = true;
    std::vector<VarInfo> pushed;
    for (size_t i = 0; i < m.params.size(); i++) {
        VarInfo vi;
        vi.name = m.params[i].second;
        vi.ty = toInfo(es.ctx, m.params[i].first);
        es.scope.push_back(vi);
        pushed.push_back(vi);
    }
    emitBodyBlock(es, *m.body);
    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
    es.curRet = savedRet;
    es.hasCurRet = savedHas;
    es.typeParams = savedTypeParams;
    es.indent--;
    es.line("}");
}

std::string methodRename(ES& es, Decl& d, Member& m, bool* isOverride, bool* isVirtual) {
    *isOverride = false;
    *isVirtual = false;
    std::string key = m.name + "/" + std::to_string(m.params.size());
    if (m.name == "main" && hasMod(m.mods, "static")) {
        if (es.ctx.mainCount == 1) return "Main";
        return "Main_" + d.name;
    }
    if (hasMod(m.mods, "static")) return m.name;
    if (m.name == "toString" && m.params.empty()) {
        *isOverride = true;
        return "ToString";
    }
    if (m.name == "hashCode" && m.params.empty()) {
        *isOverride = true;
        return "GetHashCode";
    }
    if (m.name == "equals" && m.params.size() == 1 && csTypeStr(es.ctx, m.params[0].first) == "object") {
        *isOverride = true;
        return "Equals";
    }
    if (m.name == "compareTo" && m.params.size() == 1) {
        return "CompareTo";
    }
    if (m.name == "main" && hasMod(m.mods, "static")) {
        if (es.ctx.mainCount == 1) return "Main";
        return "Main_" + d.name;
    }
    if (m.name == "iterator" && m.params.empty()) {
        bool iter = false;
        for (auto& imp : d.impls)
            if (firstSeg(imp.name) == "Iterable") iter = true;
        if (iter) return "GetEnumerator";
    }
    auto& ov = es.ctx.overrideM[d.name];
    if (ov.count(key)) {
        *isOverride = true;
        return m.name;
    }
    auto& vt = es.ctx.virtualM[d.name];
    if (vt.count(key)) *isVirtual = true;
    return m.name;
}

void emitMethod(ES& es, Decl& d, Member& m) {
    flushComments(es, m.line);
    std::string vis = visOf(m.mods);
    bool isIface = es.isInterfaceCtx;
    bool ovr = false, virt = false;
    std::string name = methodRename(es, d, m, &ovr, &virt);
    std::string modsPrefix = isIface ? "" : vis + " ";
    if (hasMod(m.mods, "static")) modsPrefix += "static ";
    else if (hasMod(m.mods, "abstract")) modsPrefix += "abstract ";
    else if (ovr) modsPrefix += "override ";
    else if (virt) modsPrefix += "virtual ";
    if (hasMod(m.mods, "synchronized")) {
        es.ctx.needCompilerServices = true;
        es.line("[MethodImpl(MethodImplOptions.Synchronized)]");
    }
    bool prevIface = es.isInterfaceCtx;
    emitMethodCommon(es, d, m, name, modsPrefix, m.body != nullptr);
    es.isInterfaceCtx = prevIface;
}

void emitCtor(ES& es, Decl& d, Member& m) {
    flushComments(es, m.line);
    emitAnnotations(es, m.anns);
    std::string vis = visOf(m.mods);
    std::string sig = vis + " " + d.name + "(" + paramListStr(es, m) + ")";
    std::string initChain;
    if (!m.baseArgs.empty()) {
        initChain = " : base(";
        for (size_t i = 0; i < m.baseArgs.size(); i++) {
            if (i) initChain += ", ";
            initChain += captureExprText(es, *m.baseArgs[i]);
        }
        initChain += ")";
    }
    es.wi();
    es.w(sig);
    if (m.body && m.body->k == SK::Block && !m.body->block.empty()) {
        Stmt& first = *m.body->block.front();
        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) {
            Expr& cc = *first.e;
            std::vector<std::string> parts;
            for (size_t i = 0; i < cc.args.size(); i++) {
                parts.push_back(captureExprText(es, *cc.args[i]));
            }
            initChain = std::string(" : ") + (cc.name == "super" ? "base(" : "this(") + joinV(parts, ", ") + ")";
            m.body->block.erase(m.body->block.begin());
        }
    }
    bool hasInitBlocks = false;
    for (auto& mm : d.members)
        if (mm.mk == 3 && !mm.isStaticInit) hasInitBlocks = true;
    es.w(initChain);
    flushTrailing(es, m.line);
    es.nl();
    es.line("{");
    es.indent++;
    if (hasInitBlocks) es.line("__InstanceInit();");
    std::vector<VarInfo> pushed;
    for (auto& p : m.params) {
        VarInfo vi;
        vi.name = p.second;
        vi.ty = toInfo(es.ctx, p.first);
        es.scope.push_back(vi);
        pushed.push_back(vi);
    }
    if (m.body) emitBodyBlock(es, *m.body);
    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
    es.indent--;
    es.line("}");
}

void emitBodyBlock(ES& es, Stmt& body) {
    if (body.k == SK::Block) {
        for (auto& s : body.block) emitStmt(es, *s);
    } else {
        emitStmt(es, body);
    }
}

void flushComments(ES& es, int line) {
    while (es.ctx.nextComment < es.ctx.comments.size()) {
        Comment& c = es.ctx.comments[es.ctx.nextComment];
        if (c.line < line) {
            if (c.block) {
                std::vector<std::string> lines = splitStr(c.text, '\n');
                for (auto& ln : lines) {
                    es.wi();
                    if (startsWith(trimS(ln), "*")) es.w(" " + trimS(ln));
                    else es.w(ln);
                    es.nl();
                }
            } else {
                es.wi();
                es.w(c.text);
                es.nl();
            }
            es.ctx.nextComment++;
        } else break;
    }
}

void flushTrailing(ES& es, int line) {
    while (es.ctx.nextComment < es.ctx.comments.size()) {
        Comment& c = es.ctx.comments[es.ctx.nextComment];
        if (c.line == line) {
            if (c.block) es.w(" /*" + c.text + "*/");
            else es.w(" " + c.text);
            es.ctx.nextComment++;
        } else break;
    }
}

void emitNestedType(ES& es, Member& m) {
    if (m.nested) emitType(es, *m.nested);
}

void emitWhere(ES& es, const std::vector<TP>& tps) {
    for (auto& tp : tps) {
        if (tp.hasBound && !tp.bound.empty()) {
            es.w(" where " + tp.name + " : " + csTypeStr(es.ctx, tp.bound));
        }
    }
}

void emitTypeHeader(ES& es, Decl& d, const std::string& kind) {
    std::string vis = visOf(d.mods);
    std::string mods = vis;
    if (hasMod(d.mods, "abstract")) mods += " abstract";
    if (hasMod(d.mods, "final")) mods += " sealed";
    es.wi();
    es.w(mods + " " + kind + " " + d.name);
    if (!d.tparams.empty()) {
        es.w("<");
        for (size_t i = 0; i < d.tparams.size(); i++) {
            if (i) es.w(", ");
            es.w(d.tparams[i].name);
        }
        es.w(">");
    }
    for (auto& tp : d.tparams) es.typeParams.insert(tp.name);
    std::string inherits;
    TypeRef effExt = d.extends;
    bool extDropped = false;
    if (!effExt.empty()) {
        std::string bs = firstSeg(effExt.name);
        if (bs == "Object") extDropped = true;
        else if (es.ctx.samDelegates.count(bs)) extDropped = true;
        if (!extDropped) inherits += csTypeStr(es.ctx, effExt);
    }
    for (auto& imp : d.impls) {
        std::string iname = firstSeg(imp.name);
        if (es.ctx.samDelegates.count(iname)) continue;
        if (iname == "Serializable" || iname == "Externalizable") continue;
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    emitWhere(es, d.tparams);
}

void emitTypeBody(ES& es, Decl& d) {
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    flushComments(es, d.line);
    bool hasInstInit = false;
    for (auto& m : d.members)
        if (m.mk == 3 && !m.isStaticInit) hasInstInit = true;
    bool instInitEmitted = false;
    for (auto& m : d.members) {
        if (m.mk == 0) emitField(es, m);
        else if (m.mk == 1) emitMethod(es, d, m);
        else if (m.mk == 2) emitCtor(es, d, m);
        else if (m.mk == 3) {
            if (m.isStaticInit || !instInitEmitted) {
                emitInitBlock(es, m, d);
                if (!m.isStaticInit) instInitEmitted = true;
            }
        } else if (m.mk == 4) emitNestedType(es, m);
    }
    if (hasInstInit) {
    }
    es.indent--;
    es.line("}");
}

void emitRecord(ES& es, Decl& d) {
    Member* compact = nullptr;
    Member* recCtor = nullptr;
    for (auto& m : d.members) {
        if (m.mk == 2 && hasMod(m.mods, "__recordctor") && !m.compactCtor) recCtor = &m;
        if (m.mk == 2 && m.compactCtor) compact = &m;
    }
    std::vector<std::pair<TypeRef, std::string>> comps;
    if (recCtor) comps = recCtor->params;
    std::string savedClsR = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " sealed class " + d.name);
    if (!d.tparams.empty()) {
        es.w("<");
        for (size_t i = 0; i < d.tparams.size(); i++) {
            if (i) es.w(", ");
            es.w(d.tparams[i].name);
        }
        es.w(">");
    }
    for (auto& tp : d.tparams) es.typeParams.insert(tp.name);
    std::string inherits;
    for (auto& imp : d.impls) {
        if (firstSeg(imp.name) == "Serializable") continue;
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    for (auto& comp : comps) {
        es.wi();
        es.w("public " + csTypeStr(es.ctx, comp.first) + " " + pascalCase(comp.second) + " { get; }");
        es.nl();
    }
    flushComments(es, d.line);
    es.wi();
    es.w("public " + d.name + "(" + paramListStr(es, *recCtor) + ")");
    es.nl();
    es.line("{");
    es.indent++;
    if (compact && compact->body) emitBodyBlock(es, *compact->body);
    for (auto& comp : comps) {
        es.wi();
        es.w(pascalCase(comp.second) + " = " + csIdent(comp.second) + ";");
        es.nl();
    }
    es.indent--;
    es.line("}");
    es.wi();
    std::string parts;
    for (auto& comp : comps) {
        if (!parts.empty()) parts += ", ";
        parts += comp.second + "={" + pascalCase(comp.second) + "}";
    }
    es.w("public override string ToString() => $\"" + d.name + "[" + parts + "]\";");
    es.nl();
    es.wi();
    std::string eq = "other is " + d.name + " p";
    for (auto& comp : comps) {
        eq += " && " + pascalCase(comp.second) + " == p." + pascalCase(comp.second);
    }
    es.w("public override bool Equals(object other) => " + eq + ";");
    es.nl();
    es.wi();
    std::string hc = "System.HashCode.Combine(";
    bool firstC = true;
    for (auto& comp : comps) {
        if (!firstC) hc += ", ";
        firstC = false;
        hc += pascalCase(comp.second);
    }
    hc += ")";
    if (comps.empty()) hc = "base.GetHashCode()";
    es.w("public override int GetHashCode() => " + hc + ";");
    es.nl();
    if (!comps.empty()) {
        es.wi();
        std::string dc;
        for (auto& comp : comps) {
            if (!dc.empty()) dc += ", ";
            dc += "out " + csTypeStr(es.ctx, comp.first) + " " + csIdent(comp.second);
        }
        es.w("public void Deconstruct(" + dc + ")");
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& comp : comps) {
            es.wi();
            es.w(csIdent(comp.second) + " = " + pascalCase(comp.second) + ";");
            es.nl();
        }
        es.indent--;
        es.line("}");
    }
    for (auto& m : d.members) {
        if (m.mk == 2 && (hasMod(m.mods, "__recordctor") || m.compactCtor)) continue;
        if (m.mk == 0) emitField(es, m);
        else if (m.mk == 1) emitMethod(es, d, m);
        else if (m.mk == 2) emitCtor(es, d, m);
        else if (m.mk == 4) emitNestedType(es, m);
    }
    es.indent--;
    es.line("}");
    es.cls = savedClsR;
}

void emitEnumSimple(ES& es, Decl& d) {
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " enum " + d.name);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    for (size_t i = 0; i < d.consts.size(); i++) {
        flushComments(es, d.consts[i].line);
        es.wi();
        es.w(csIdent(d.consts[i].name));
        if (i + 1 < d.consts.size()) es.w(",");
        flushTrailing(es, d.consts[i].line);
        es.nl();
    }
    es.indent--;
    es.line("}");
}

void emitEnumClass(ES& es, Decl& d) {
    std::string savedCls = es.cls;
    es.cls = d.name;
    std::string savedClsR = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " class " + d.name);
    std::string inherits;
    for (auto& imp : d.impls) {
        if (firstSeg(imp.name) == "Serializable") continue;
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    es.line("private static int __ord = 0;");
    es.line("public readonly int Ordinal;");
    es.line("public readonly string Name;");
    std::vector<Member> ctors;
    bool hasCtor = false;
    for (auto& m : d.members)
        if (m.mk == 2) hasCtor = true;
    for (auto& m : d.members) {
        if (m.mk == 0) emitField(es, m);
    }
    if (!hasCtor) {
        es.wi();
        es.w("public " + d.name + "(string __name)");
        es.nl();
        es.line("{");
        es.indent++;
        es.line("Ordinal = __ord++;");
        es.line("Name = __name;");
        es.indent--;
        es.line("}");
    } else {
        for (auto& m : d.members) {
            if (m.mk != 2) continue;
            flushComments(es, m.line);
            es.wi();
            es.w("public " + d.name + "(string __name" +
                 (m.params.empty() ? "" : ", " + paramListStr(es, m)) + ")");
            es.nl();
            es.line("{");
            es.indent++;
            es.line("Ordinal = __ord++;");
            es.line("Name = __name;");
            std::vector<VarInfo> pushed;
            for (auto& p : m.params) {
                VarInfo vi;
                vi.name = p.second;
                vi.ty = toInfo(es.ctx, p.first);
                es.scope.push_back(vi);
                pushed.push_back(vi);
            }
            if (m.body) emitBodyBlock(es, *m.body);
            for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
            es.indent--;
            es.line("}");
        }
    }
    for (auto& c : d.consts) {
        flushComments(es, c.line);
        es.wi();
        std::string ctorCall;
        if (hasCtor) {
            ctorCall = "new " + d.name + "(\"" + c.name + "\"";
            for (auto& a : c.args) {
                ctorCall += ", ";
                ctorCall += captureExprText(es, *a);
            }
            ctorCall += ")";
        } else {
            ctorCall = "new " + d.name + "(\"" + c.name + "\")";
        }
        if (c.body) {
            es.w("public static readonly " + d.name + " " + csIdent(c.name) + " = new __E_" + c.name + "(\"" + c.name + "\"");
            for (auto& a : c.args) {
                es.w(", ");
                emitExpr(es, *a);
            }
            es.w(");");
            flushTrailing(es, c.line);
            es.nl();
        } else {
            es.w("public static readonly " + d.name + " " + csIdent(c.name) + " = " + ctorCall + ";");
            flushTrailing(es, c.line);
            es.nl();
        }
    }
    for (auto& c : d.consts) {
        if (c.body) {
            flushComments(es, c.line);
            es.wi();
            es.w("private sealed class __E_" + c.name + " : " + d.name);
            es.nl();
            es.line("{");
            es.indent++;
            es.wi();
            const Member* matchCtor = nullptr;
            for (auto& m : d.members)
                if (m.mk == 2 && m.params.size() == c.args.size()) {
                    matchCtor = &m;
                    break;
                }
            std::string ps = "string __n";
            std::string baseArgs;
            if (matchCtor) {
                for (auto& p : matchCtor->params) {
                    ps += ", " + csTypeStr(es.ctx, p.first) + " " + csIdent(p.second);
                    baseArgs += ", " + csIdent(p.second);
                }
            }
            es.w("public __E_" + c.name + "(" + ps + ") : base(__n" + baseArgs + ")");
            es.nl();
            es.line("{");
            es.indent++;
            if (c.body) {
                for (auto& m : c.body->members) {
                    if (m.mk == 3 && m.body) emitBodyBlock(es, *m.body);
                }
            }
            es.indent--;
            es.line("}");
            for (auto& m : c.body->members) {
                if (m.mk == 1) {
                    bool ovr = false, virt = false;
                    std::string name = methodRename(es, d, m, &ovr, &virt);
                    if (!ovr)
                        for (auto& em : d.members)
                            if (em.mk == 1 && em.name == m.name && em.params.size() == m.params.size() &&
                                !hasMod(em.mods, "static"))
                                ovr = true;
                    std::string modsPrefix = "public ";
                    if (ovr) modsPrefix += "override ";
                    else modsPrefix += "virtual ";
                    bool hasWhere = false;
                    std::string tp = tparamsStr(es, m.tparams, &hasWhere);
                    std::set<std::string> savedTypeParams = es.typeParams;
                    for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
                    es.wi();
                    es.w(modsPrefix + csTypeStr(es.ctx, m.ty) + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")");
                    es.nl();
                    es.line("{");
                    es.indent++;
                    std::vector<VarInfo> pushed;
                    for (auto& p : m.params) {
                        VarInfo vi;
                        vi.name = p.second;
                        vi.ty = toInfo(es.ctx, p.first);
                        es.scope.push_back(vi);
                        pushed.push_back(vi);
                    }
                    if (m.body) emitBodyBlock(es, *m.body);
                    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
                    es.typeParams = savedTypeParams;
                    es.indent--;
                    es.line("}");
                }
            }
            es.indent--;
            es.line("}");
        }
    }
    bool instInit = false;
    for (auto& m : d.members)
        if (m.mk == 3 && !m.isStaticInit) instInit = true;
    for (auto& m : d.members) {
        if (m.mk == 3) {
            emitInitBlock(es, m, d);
        }
        if (m.mk == 1) {
            bool ovr = false, virt = false;
            std::string name = methodRename(es, d, m, &ovr, &virt);
            bool byConst = false;
            if (!hasMod(m.mods, "static"))
                for (auto& c : d.consts)
                    if (c.body)
                        for (auto& bm : c.body->members)
                            if (bm.mk == 1 && bm.name == m.name && bm.params.size() == m.params.size())
                                byConst = true;
            std::string modsPrefix = "public ";
            if (hasMod(m.mods, "static")) modsPrefix += "static ";
            else if (hasMod(m.mods, "abstract")) modsPrefix += "abstract ";
            else if (ovr) modsPrefix += "override ";
            else if (virt || byConst) modsPrefix += "virtual ";
            bool hasWhere = false;
            std::string tp = tparamsStr(es, m.tparams, &hasWhere);
            std::set<std::string> savedTypeParams = es.typeParams;
            for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
            flushComments(es, m.line);
            es.wi();
            es.w(modsPrefix + csTypeStr(es.ctx, m.ty) + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")");
            flushTrailing(es, m.line);
            es.nl();
            if (!m.body) {
                es.wi();
                es.w(";");
                es.nl();
            } else {
                es.line("{");
                es.indent++;
                std::vector<VarInfo> pushed;
                for (auto& p : m.params) {
                    VarInfo vi;
                    vi.name = p.second;
                    vi.ty = toInfo(es.ctx, p.first);
                    es.scope.push_back(vi);
                    pushed.push_back(vi);
                }
                emitBodyBlock(es, *m.body);
                for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
                es.typeParams = savedTypeParams;
                es.indent--;
                es.line("}");
            }
        }
        if (m.mk == 4) emitNestedType(es, m);
    }
    bool userToString = false;
    bool userCompareTo = false;
    for (auto& m : d.members) {
        if (m.mk == 1 && m.name == "toString" && m.params.empty()) userToString = true;
        if (m.mk == 1 && m.name == "compareTo" && m.params.size() == 1) userCompareTo = true;
    }
    if (!userToString) es.line("public override string ToString() => Name;");
    if (!userCompareTo) es.line("public int CompareTo(" + d.name + " other) => Ordinal - other.Ordinal;");
    std::string vals;
    for (size_t i = 0; i < d.consts.size(); i++) {
        if (i) vals += ", ";
        vals += csIdent(d.consts[i].name);
    }
    es.line("public static " + d.name + "[] Values() => new " + d.name + "[] { " + vals + " };");
    es.line("public static " + d.name + " ValueOf(string name) => Array.Find(Values(), c => c.Name == name);");
    es.indent--;
    es.line("}");
    es.cls = savedCls;
    (void)instInit;
}

void emitAnnotationDecl(ES& es, Decl& d) {
    flushComments(es, d.line);
    es.wi();
    es.w(visOf(d.mods) + " class " + d.name + " : Attribute");
    es.nl();
    es.line("{");
    es.indent++;
    for (auto& m : d.members) {
        if (m.mk == 1) {
            es.wi();
            std::string def;
            if (m.body && m.body->k == SK::Block && m.body->block.size() == 1 &&
                m.body->block[0]->k == SK::Return && m.body->block[0]->e) {
                def = " = " + captureExprText(es, *m.body->block[0]->e);
            }
            es.w("public " + csTypeStr(es.ctx, m.ty) + " " + pascalCase(m.name) + " { get; set; }" + (def.empty() ? "" : def + ";"));
            es.nl();
        }
    }
    es.indent--;
    es.line("}");
}

void emitInterface(ES& es, Decl& d) {
    std::string savedCls = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " interface " + d.name);
    if (!d.tparams.empty()) {
        es.w("<");
        for (size_t i = 0; i < d.tparams.size(); i++) {
            if (i) es.w(", ");
            es.w(d.tparams[i].name);
        }
        es.w(">");
    }
    for (auto& tp : d.tparams) es.typeParams.insert(tp.name);
    std::string inherits;
    if (!d.extends.empty()) {
        inherits += csTypeStr(es.ctx, d.extends);
    }
    for (auto& imp : d.impls) {
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    emitWhere(es, d.tparams);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    bool prev = es.isInterfaceCtx;
    es.isInterfaceCtx = true;
    for (auto& m : d.members) {
        if (m.mk == 0) {
            bool constAble = isConstAble(es, m);
            for (size_t i = 0; i < m.names.size(); i++) {
                flushComments(es, m.line);
                es.wi();
                std::string t = csTypeStr(es.ctx, m.ty);
                for (int k = 0; k < m.fdims[i]; k++) t += "[]";
                std::string mods = constAble ? "const " : "static readonly ";
                es.w(mods + t + " " + csIdent(m.names[i]));
                if (m.finits[i]) {
                    es.w(" = ");
                    TypeInfo expect;
                    expect.known = true;
                    expect.name = csTypeStr(es.ctx, m.ty);
                    emitExpr(es, *m.finits[i], 0, expect);
                }
                es.w(";");
                es.nl();
            }
        } else if (m.mk == 1) {
            bool ovr = false, virt = false;
            std::string name = methodRename(es, d, m, &ovr, &virt);
            std::string modsPrefix;
            if (hasMod(m.mods, "static")) modsPrefix = "static ";
            else if (hasMod(m.mods, "private")) modsPrefix = "private ";
            bool hasWhere = false;
            std::string tp = tparamsStr(es, m.tparams, &hasWhere);
            std::set<std::string> savedTypeParams = es.typeParams;
            for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
            flushComments(es, m.line);
            es.wi();
            es.w(modsPrefix + csTypeStr(es.ctx, m.ty) + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")");
            es.nl();
            if (m.body) {
                es.line("{");
                es.indent++;
                std::vector<VarInfo> pushed;
                for (auto& p : m.params) {
                    VarInfo vi;
                    vi.name = p.second;
                    vi.ty = toInfo(es.ctx, p.first);
                    es.scope.push_back(vi);
                    pushed.push_back(vi);
                }
                emitBodyBlock(es, *m.body);
                for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
                es.indent--;
                es.line("}");
            } else {
                es.wi();
                es.w(";");
                es.nl();
            }
            es.typeParams = savedTypeParams;
        } else if (m.mk == 4) {
            emitNestedType(es, m);
        }
    }
    es.isInterfaceCtx = prev;
    es.indent--;
    es.line("}");
}

void emitClass(ES& es, Decl& d) {
    std::string savedCls = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    emitTypeHeader(es, d, "class");
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    bool prev = es.isInterfaceCtx;
    es.isInterfaceCtx = false;
    bool instInitEmitted = false;
    for (auto& m : d.members) {
        if (m.mk == 0) emitField(es, m);
        else if (m.mk == 1) emitMethod(es, d, m);
        else if (m.mk == 2) emitCtor(es, d, m);
        else if (m.mk == 3) {
            if (m.isStaticInit || !instInitEmitted) {
                emitInitBlock(es, m, d);
                if (!m.isStaticInit) instInitEmitted = true;
            }
        } else if (m.mk == 4) emitNestedType(es, m);
    }
    es.isInterfaceCtx = prev;
    es.indent--;
    es.line("}");
    es.cls = savedCls;
}

void emitType(ES& es, Decl& d) {
    if (d.dk == 1 && es.ctx.samDelegates.count(d.name)) {
        std::string sig = delegateSigFor(es.ctx, d);
        if (!sig.empty()) {
            flushComments(es, d.line);
            emitAnnotations(es, d.anns);
            es.wi();
            es.w(visOf(d.mods) + " delegate " + sig + ";");
            es.nl();
            return;
        }
    }
    if (d.dk == 0) emitClass(es, d);
    else if (d.dk == 1) emitInterface(es, d);
    else if (d.dk == 2) {
        if (es.ctx.simpleEnums.count(d.name)) emitEnumSimple(es, d);
        else emitEnumClass(es, d);
    } else if (d.dk == 3) emitRecord(es, d);
    else if (d.dk == 4) emitAnnotationDecl(es, d);
}

std::string delegateSigFor(Ctx& ctx, Decl& iface) {
    for (auto& m : iface.members) {
        if (m.mk == 1 && !m.body) {
            std::string r;
            r = csTypeStr(ctx, m.ty) + " " + iface.name + "(";
            for (size_t i = 0; i < m.params.size(); i++) {
                if (i) r += ", ";
                if (m.varargs && i == m.params.size() - 1) {
                    TypeRef at = m.params[i].first;
                    if (at.dims == 0) at.dims = 1;
                    r += "params " + csTypeStr(ctx, at) + " " + m.params[i].second;
                } else {
                    r += csTypeStr(ctx, m.params[i].first) + " " + m.params[i].second;
                }
            }
            r += ")";
            return r;
        }
    }
    return "";
}

void flushRemainingComments(Ctx& ctx, std::ostringstream& out) {
    while (ctx.nextComment < ctx.comments.size()) {
        Comment& c = ctx.comments[ctx.nextComment++];
        if (c.block) out << "/*" << c.text << "*/\n";
        else out << c.text << "\n";
    }
}

std::string convertUnit(Ctx& ctx, Unit& u) {
    ES es(ctx);
    for (auto& d : u.types) emitType(es, *d);
    std::string helpers;
    if (ctx.needCollectionsSpec) {
        ctx.needLinq = true;
        helpers += R"hlp(static class __JHelp
{
    public static T Poll<T>(LinkedList<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q.First.Value;
        q.RemoveFirst();
        return v;
    }

    public static T Poll<T>(List<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q[0];
        q.RemoveAt(0);
        return v;
    }

    public static T PollLast<T>(LinkedList<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q.Last.Value;
        q.RemoveLast();
        return v;
    }

    public static T PollLast<T>(List<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q[q.Count - 1];
        q.RemoveAt(q.Count - 1);
        return v;
    }

    public static T PollPQ<T>(List<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q.Min();
        q.Remove(v);
        return v;
    }

    public static T[] CopyOf<T>(T[] a, int n)
    {
        T[] d = new T[Math.Max(0, n)];
        Array.Copy(a, d, Math.Min(a.Length, Math.Max(0, n)));
        return d;
    }

    public static T[] CopyOfRange<T>(T[] a, int from, int to)
    {
        T[] d = new T[Math.Max(0, to - from)];
        for (int i = from; i < to && i < a.Length; i++) d[i - from] = a[i];
        return d;
    }

    public static IEnumerable<int> IntRange(int start, int end, bool inclusive)
    {
        for (long value = start; inclusive ? value <= end : value < end; value++)
            yield return (int)value;
    }

    public static IEnumerable<long> LongRange(long start, long end, bool inclusive)
    {
        for (long value = start; inclusive ? value <= end : value < end;)
        {
            yield return value;
            if (value == long.MaxValue) yield break;
            value++;
        }
    }

    public static List<T> AsList<T>(params T[] a)
    {
        return new List<T>(a);
    }

    public static string ArrayToString<T>(T[] a)
    {
        return "[" + string.Join(", ", a) + "]";
    }

    public static bool ArrayEquals<T>(T[] a, T[] b)
    {
        return a.SequenceEqual(b);
    }

    public static void Sort<T>(List<T> l)
    {
        l.Sort();
    }

    public static void Sort<T>(List<T> l, Comparison<T> c)
    {
        l.Sort(c);
    }

    public static void Reverse<T>(List<T> l)
    {
        l.Reverse();
    }

    public static void Swap<T>(IList<T> l, int i, int j)
    {
        (l[i], l[j]) = (l[j], l[i]);
    }
}

)hlp";
    }
    if (ctx.needScanner) {
        helpers += R"hlp(class __Scanner
{
    readonly System.IO.TextReader r;
    string[] toks = new string[0];
    int pos = 0;
    bool midLine = false;

    public __Scanner(System.IO.TextReader src) { r = src; }

    bool refill()
    {
        while (pos >= toks.Length)
        {
            string line = r.ReadLine();
            if (line == null) { toks = new string[0]; pos = 0; return false; }
            toks = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
            pos = 0;
            midLine = false;
        }
        return true;
    }

    public string next() { if (!refill()) return null; midLine = true; return toks[pos++]; }
    public bool hasNext() { return refill(); }
    public bool hasNextLine() { return pos < toks.Length || r.Peek() != -1; }
    public int nextInt() { return int.Parse(next()); }
    public long nextLong() { return long.Parse(next()); }
    public double nextDouble() { return double.Parse(next(), System.Globalization.CultureInfo.InvariantCulture); }
    public bool nextBoolean() { return bool.Parse(next()); }

    public string nextLine()
    {
        if (midLine)
        {
            if (pos >= toks.Length) { midLine = false; return ""; }
            string rest = string.Join(" ", toks, pos, toks.Length - pos);
            pos = toks.Length;
            midLine = false;
            return rest;
        }
        string line = r.ReadLine();
        return line == null ? "" : line;
    }

    public void close() { }
}

)hlp";
    }
    std::string body = helpers + es.out.str();
    while (!body.empty() && body.back() == '\n') body.pop_back();
    body += "\n";
    std::set<std::string> usings;
    usings.insert("System");
    usings.insert("System.Collections.Generic");
    if (ctx.needLinq) usings.insert("System.Linq");
    if (ctx.needText) usings.insert("System.Text");
    if (ctx.needIO) usings.insert("System.IO");
    if (ctx.needRegex) usings.insert("System.Text.RegularExpressions");
    if (ctx.needDiag) usings.insert("System.Diagnostics");
    if (ctx.needCompilerServices) usings.insert("System.Runtime.CompilerServices");
    if (ctx.needGlobalization) usings.insert("System.Globalization");
    if (ctx.needNumerics) usings.insert("System.Numerics");
    if (ctx.needThreading) usings.insert("System.Threading");
    if (ctx.needReflection) usings.insert("System.Reflection");
    for (auto& ns : ctx.usings) {
        if (ns == "System" || ns == "System.Collections.Generic") continue;
        usings.insert(ns);
    }
    std::ostringstream finalOut;
    for (auto& us : usings) finalOut << "using " << us << ";\n";
    finalOut << "\n";
    if (!u.package.empty()) {
        finalOut << "namespace " << u.package << "\n{\n";
        std::istringstream in(body);
        std::string ln;
        while (std::getline(in, ln)) {
            if (ln.empty()) finalOut << "\n";
            else finalOut << "    " << ln << "\n";
        }
        finalOut << "}\n";
    } else {
        finalOut << body;
    }
    flushRemainingComments(ctx, finalOut);
    return finalOut.str();
}
bool stmtTerminates(Stmt& st);
bool stmtTerminatesOrBreak(CaseArm& arm);
void emitArgs(ES& es, std::vector<std::unique_ptr<Expr>>& args);
bool emitExprStmt(ES& es, Expr& e);
void emitLocalVar(ES& es, Stmt& st);
void emitFor(ES& es, Stmt& st);
void emitForeach(ES& es, Stmt& st);
bool switchENeedsStmtForm(Expr& e);
void emitSwitchEAsStmt(ES& es, Expr& sw, const std::string& target);

bool stmtTerminates(Stmt& st) {
    switch (st.k) {
        case SK::Return:
        case SK::Throw:
        case SK::Break:
        case SK::Continue:
            return true;
        case SK::Block:
            if (st.block.empty()) return false;
            return stmtTerminates(*st.block.back());
        case SK::If:
            return st.body && st.body2 && stmtTerminates(*st.body) && stmtTerminates(*st.body2);
        case SK::Label:
            return st.body && stmtTerminates(*st.body);
        case SK::Switch: {
            if (st.arms.empty()) return false;
            bool hasDefault = false;
            for (auto& arm : st.arms) {
                if (arm.arrow) {
                    if (arm.isBlockVal) {
                        if (arm.body.empty() || !stmtTerminates(*arm.body.back())) return false;
                    } else return false;
                } else {
                    if (arm.body.empty() || !stmtTerminates(*arm.body.back())) return false;
                }
                if (arm.isDefault) hasDefault = true;
            }
            return hasDefault;
        }
        case SK::Try:
            if (st.fin) return stmtTerminates(*st.fin);
            return false;
        case SK::Sync:
            return st.body && stmtTerminates(*st.body);
        default:
            return false;
    }
}

void emitBodyStmt(ES& es, Stmt& st) {
    if (st.k == SK::Block) {
        es.line("{");
        es.indent++;
        for (auto& s : st.block) emitStmt(es, *s);
        es.indent--;
        es.line("}");
    } else {
        es.indent++;
        emitStmt(es, st);
        es.indent--;
    }
}

std::string captureExpr(ES& es, Expr& e, int prec = 0, const TypeInfo& expect = TypeInfo()) {
    return captureExprText(es, e, prec, expect);
}

std::string enumLabelText(ES& es, Expr& l, const std::string& subjType) {
    std::string lt = captureExpr(es, l, 20);
    if (!subjType.empty() && l.k == EK::Ident) {
        auto it = es.ctx.enumConstOwner.find(l.name);
        if (it != es.ctx.enumConstOwner.end() && it->second == subjType) return subjType + "." + lt;
    }
    return lt;
}

void emitEnumClassSwitch(ES& es, Stmt& st, const std::string& enumName) {
    std::string subj = captureExpr(es, *st.subject, 20);
    es.wi();
    es.w("{");
    es.nl();
    es.indent++;
    std::string swv = "__sw" + std::to_string(es.tmpCounter);
    es.tmpCounter++;
    es.wi();
    es.w("var " + swv + " = " + subj + ";");
    es.nl();
    struct SwSection {
        std::vector<Expr*> labels;
        const std::vector<TypeRef>* patLabels = nullptr;
        const std::string* patName = nullptr;
        bool nullLabel = false;
        Expr* guard = nullptr;
        bool isDefault = false;
        std::vector<std::unique_ptr<Stmt>>* body = nullptr;
        bool arrow = false;
        Expr* value = nullptr;
    };
    std::vector<SwSection> sections;
    std::vector<Expr*> pending;
    for (auto& arm : st.arms) {
        bool labelOnly = !arm.arrow && arm.body.empty() && !arm.isDefault && !arm.nullLabel &&
                         arm.patLabels.empty() && !arm.labels.empty() && !arm.guard && !arm.value;
        if (labelOnly) {
            for (auto& l : arm.labels) pending.push_back(l.get());
            continue;
        }
        SwSection sec;
        for (auto& l : pending) sec.labels.push_back(l);
        pending.clear();
        for (auto& l : arm.labels) sec.labels.push_back(l.get());
        sec.patLabels = &arm.patLabels;
        sec.patName = &arm.patName;
        sec.nullLabel = arm.nullLabel;
        sec.guard = arm.guard.get();
        sec.isDefault = arm.isDefault;
        sec.body = &arm.body;
        sec.arrow = arm.arrow;
        sec.value = arm.value.get();
        sections.push_back(std::move(sec));
    }
    SwSection* defaultArm = nullptr;
    bool firstSec = true;
    for (auto& sec : sections) {
        if (sec.isDefault && sec.labels.empty() && sec.patLabels->empty() && !sec.nullLabel) {
            defaultArm = &sec;
            continue;
        }
        std::vector<std::string> conds;
        for (auto* l : sec.labels) conds.push_back(swv + " == " + enumLabelText(es, *l, enumName));
        if (sec.nullLabel) conds.push_back(swv + " == null");
        for (auto& pl : *sec.patLabels)
            conds.push_back(swv + " is " + csTypeStr(es.ctx, pl, true) +
                            (sec.patName->empty() ? " _" : " " + csIdent(*sec.patName)));
        es.wi();
        es.w(firstSec ? "if (" : "else if (");
        firstSec = false;
        bool firstL = true;
        for (auto& c : conds) {
            if (!firstL) es.w(" || ");
            firstL = false;
            es.w(c);
        }
        es.w(")");
        if (sec.guard) {
            std::string g = captureExpr(es, *sec.guard, 0);
            es.w(firstL ? "(" : " && (");
            es.w(g);
            es.w(")");
        }
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& s : *sec.body) {
            if (s->k == SK::Break && s->label.empty()) continue;
            emitStmt(es, *s);
        }
        if (sec.arrow && sec.value) {
            es.wi();
            if (!emitExprStmt(es, *sec.value)) es.w(";");
            es.nl();
        }
        es.indent--;
        es.line("}");
    }
    if (defaultArm) {
        es.wi();
        es.w("else");
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& s : *defaultArm->body) {
            if (s->k == SK::Break && s->label.empty()) continue;
            emitStmt(es, *s);
        }
        es.indent--;
        es.line("}");
    }
    es.indent--;
    es.line("}");
}

void emitClassicSwitch(ES& es, Stmt& st) {
    TypeInfo subjTy = inferExprType(es, *st.subject);
    if (subjTy.known && es.ctx.enumClassTypes.count(subjTy.name)) {
        emitEnumClassSwitch(es, st, subjTy.name);
        return;
    }
    if (subjTy.known && es.ctx.simpleEnums.count(subjTy.name)) es.switchSubjectType = subjTy.name;
    std::string subj = captureExpr(es, *st.subject, 20);
    es.wi();
    es.w("switch (" + subj + ")");
    es.nl();
    es.line("{");
    es.indent++;
    std::vector<std::string> pendingLabels;
    for (size_t i = 0; i < st.arms.size(); i++) {
        CaseArm& arm = st.arms[i];
        if (!arm.arrow && arm.body.empty() && !arm.isDefault && !arm.nullLabel &&
            arm.patLabels.empty() && !arm.labels.empty()) {
            for (auto& l : arm.labels) pendingLabels.push_back(enumLabelText(es, *l, es.switchSubjectType));
            continue;
        }
        es.wi();
        bool first = true;
        if (arm.isDefault) {
            es.w("default:");
            first = false;
        }
        if (arm.nullLabel) {
            if (!first) es.w(" ");
            es.w("case null:");
            first = false;
        }
        for (auto& pl : arm.patLabels) {
            if (!first) es.w(" ");
            first = false;
            es.w("case " + csTypeStr(es.ctx, pl, true) +
                 (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)) + ":");
        }
        for (auto& l : arm.labels) {
            if (!first) es.w(" ");
            first = false;
            es.w("case " + enumLabelText(es, *l, es.switchSubjectType) + ":");
        }
        for (auto& pl : pendingLabels) es.w(" case " + pl + ":");
        pendingLabels.clear();
        if (arm.guard) es.w(" when " + captureExpr(es, *arm.guard, 20));
        es.nl();
        es.indent++;
        bool bodyTerm = !arm.body.empty() && stmtTerminates(*arm.body.back());
        for (auto& s : arm.body) emitStmt(es, *s);
        if (arm.arrow && arm.value) {
            auto exprStmt = std::make_unique<Stmt>();
            exprStmt->k = SK::ExprStmt;
            exprStmt->e = std::move(arm.value);
            emitStmt(es, *exprStmt);
            bodyTerm = false;
        }
        if (!bodyTerm) {
            std::string gotoTarget;
            bool nextExists = false;
            for (size_t j = i + 1; j < st.arms.size(); j++) {
                CaseArm& nx = st.arms[j];
                if (nx.isDefault) {
                    nextExists = true;
                    gotoTarget = "default";
                    break;
                }
                if (!nx.labels.empty()) {
                    nextExists = true;
                    gotoTarget = captureExpr(es, *nx.labels.front(), 20);
                    break;
                }
            }
            if (nextExists) {
                if (gotoTarget == "default") es.line("goto default;");
                else es.line("goto case " + gotoTarget + ";");
            } else {
                es.line("break;");
            }
        }
        es.indent--;
    }
    es.indent--;
    es.line("}");
    es.switchSubjectType = "";
}

bool stmtTerminatesOrBreak(CaseArm& arm) {
    if (arm.body.empty()) return false;
    Stmt& last = *arm.body.back();
    if (last.k == SK::Break && last.label.empty()) return true;
    return stmtTerminates(last);
}

void emitSwitch(ES& es, Stmt& st) {
    flushComments(es, st.line);
    TypeInfo subjTy = inferExprType(es, *st.subject);
    if (subjTy.known && es.ctx.enumClassTypes.count(subjTy.name)) {
        emitClassicSwitch(es, st);
        es.switchSubjectType = "";
        flushTrailing(es, st.line);
        return;
    }
    if (subjTy.known && es.ctx.simpleEnums.count(subjTy.name)) es.switchSubjectType = subjTy.name;
    if (st.arrowSw) {
        std::string subj = captureExpr(es, *st.subject, 20);
        es.wi();
        es.w("switch (" + subj + ")");
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& arm : st.arms) {
            if (!arm.labels.empty() || !arm.patLabels.empty() || arm.nullLabel || arm.isDefault) {
                es.wi();
                bool first = true;
                if (arm.guard) {
                    es.w("case ");
                    bool g1 = true;
                    if (arm.isDefault) {
                        es.w("_");
                        g1 = false;
                    }
                    if (arm.nullLabel) {
                        if (!g1) es.w(" or ");
                        es.w("null");
                        g1 = false;
                    }
                    for (auto& pl : arm.patLabels) {
                        if (!g1) es.w(" or ");
                        g1 = false;
                        es.w(csTypeStr(es.ctx, pl, true) +
                             (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)));
                    }
                    for (auto& l : arm.labels) {
                        if (!g1) es.w(" or ");
                        g1 = false;
                        es.w(enumLabelText(es, *l, es.switchSubjectType));
                    }
                    if (g1) es.w("_");
                    es.w(" when " + captureExpr(es, *arm.guard, 20));
                    es.w(":");
                } else {
                    if (arm.isDefault) {
                        es.w("default:");
                        first = false;
                    }
                    if (arm.nullLabel) {
                        if (!first) es.w(" ");
                        es.w("case null:");
                        first = false;
                    }
                    for (auto& pl : arm.patLabels) {
                        if (!first) es.w(" ");
                        es.w("case " + csTypeStr(es.ctx, pl, true) +
                             (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)) + ":");
                        first = false;
                    }
                    for (auto& l : arm.labels) {
                        if (!first) es.w(" ");
                        first = false;
                        es.w("case " + enumLabelText(es, *l, es.switchSubjectType) + ":");
                    }
                    if (first) es.w("default:");
                }
                es.nl();
            }
            es.indent++;
            bool term = false;
            if (arm.value) {
                auto exprStmt = std::make_unique<Stmt>();
                exprStmt->k = SK::ExprStmt;
                exprStmt->e = std::move(arm.value);
                emitStmt(es, *exprStmt);
            }
            for (auto& s : arm.body) {
                emitStmt(es, *s);
            }
            if (!arm.body.empty()) {
                Stmt& last = *arm.body.back();
                term = last.k == SK::Break || last.k == SK::Return || last.k == SK::Throw ||
                       last.k == SK::Continue;
            }
            if (!term) es.line("break;");
            es.indent--;
        }
        es.indent--;
        es.line("}");
        es.switchSubjectType = "";
        flushTrailing(es, st.line);
        return;
    }
    emitClassicSwitch(es, st);
    flushTrailing(es, st.line);
}

void emitTry(ES& es, Stmt& st) {
    flushComments(es, st.line);
    size_t depth = 0;
    for (size_t i = 0; i < st.resources.size(); i++) {
        es.wi();
        std::string rt = csTypeStr(es.ctx, st.resources[i].first);
        if (rt == "var") rt = "var";
        es.w("using (" + rt + " " + csIdent(st.resources[i].second) + " = " +
             captureExpr(es, *st.resInits[i], 0) + ")");
        es.nl();
        es.line("{");
        es.indent++;
        depth++;
        es.scope.push_back({st.resources[i].second, toInfo(es.ctx, st.resources[i].first)});
    }
    bool needTry = !st.catches.empty() || st.fin;
    if (needTry) {
        es.wi();
        es.w("try");
        es.nl();
        es.wi();
        es.w("{");
        es.nl();
        es.indent++;
        if (st.body) emitBodyBlock(es, *st.body);
        es.indent--;
        es.wi();
        es.w("}");
        es.nl();
        for (auto& c : st.catches) {
            es.wi();
            if (c.types.size() == 1) {
                es.w("catch (" + csTypeStr(es.ctx, c.types[0]) + " " + csIdent(c.var) + ")");
            } else {
                std::string cond;
                for (size_t i = 0; i < c.types.size(); i++) {
                    if (i) cond += " || ";
                    cond += csIdent(c.var) + " is " + csTypeStr(es.ctx, c.types[i]);
                }
                es.w("catch (Exception " + csIdent(c.var) + ") when (" + cond + ")");
            }
            es.nl();
            es.wi();
            es.w("{");
            es.nl();
            es.indent++;
            {
                TypeInfo ct;
                ct.known = true;
                ct.name = c.types.size() == 1 ? csTypeStr(es.ctx, c.types[0]) : "Exception";
                es.scope.push_back({c.var, ct});
            }
            if (c.body) emitBodyBlock(es, *c.body);
            es.scope.pop_back();
            es.indent--;
            es.wi();
            es.w("}");
            es.nl();
        }
        if (st.fin) {
            es.wi();
            es.w("finally");
            es.nl();
            es.wi();
            es.w("{");
            es.nl();
            es.indent++;
            emitBodyBlock(es, *st.fin);
            es.indent--;
            es.wi();
            es.w("}");
            es.nl();
        }
    } else {
        if (st.body) emitBodyStmt(es, *st.body);
    }
    for (size_t i = 0; i < depth; i++) {
        es.indent--;
        es.line("}");
        if (!es.scope.empty()) es.scope.pop_back();
    }
    flushTrailing(es, st.line);
}

void emitStmt(ES& es, Stmt& st) {
    switch (st.k) {
        case SK::Block:
            flushComments(es, st.line);
            es.line("{");
            es.indent++;
            for (auto& s : st.block) emitStmt(es, *s);
            es.indent--;
            es.line("}");
            break;
        case SK::Empty:
            flushComments(es, st.line);
            es.wi();
            es.w(";");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::LocalClass:
            flushComments(es, st.line);
            break;
        case SK::ExprStmt:
            flushComments(es, st.line);
            es.wi();
            if (st.e) {
                if (!emitExprStmt(es, *st.e)) es.w(";");
            }
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::LocalVar:
            emitLocalVar(es, st);
            break;
        case SK::If:
            flushComments(es, st.line);
            es.wi();
            es.w("if (" + captureExpr(es, *st.e, 0) + ")");
            flushTrailing(es, st.line);
            es.nl();
            emitBodyStmt(es, *st.body);
            if (st.body2) {
                es.wi();
                es.w("else");
                es.nl();
                emitBodyStmt(es, *st.body2);
            }
            break;
        case SK::While:
            flushComments(es, st.line);
            es.wi();
            es.w("while (" + captureExpr(es, *st.e, 0) + ")");
            flushTrailing(es, st.line);
            es.nl();
            emitBodyStmt(es, *st.body);
            break;
        case SK::DoWhile:
            flushComments(es, st.line);
            es.wi();
            es.w("do");
            es.nl();
            emitBodyStmt(es, *st.body);
            es.wi();
            es.w("while (" + captureExpr(es, *st.e, 0) + ");");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::For:
            emitFor(es, st);
            break;
        case SK::Foreach:
            emitForeach(es, st);
            break;
        case SK::Break:
            flushComments(es, st.line);
            es.wi();
            if (st.label.empty()) es.w("break;");
            else if (es.activeLabels.count(st.label)) es.w("goto " + csIdent(st.label) + "__end;");
            else es.w("break;");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Continue:
            flushComments(es, st.line);
            es.wi();
            if (st.label.empty()) es.w("continue;");
            else if (es.activeLabels.count(st.label)) es.w("goto " + csIdent(st.label) + "__cont;");
            else es.w("continue;");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Return:
            flushComments(es, st.line);
            es.wi();
            if (st.e && st.e->k == EK::SwitchE && switchENeedsStmtForm(*st.e) && es.hasCurRet &&
                es.curRet.name != "void" && !es.curRet.empty() && !es.curRet.isVar) {
                std::string tmp = "__swret" + std::to_string(es.tmpCounter++);
                es.w(csTypeStr(es.ctx, es.curRet) + " " + tmp + " = default;");
                es.nl();
                emitSwitchEAsStmt(es, *st.e, tmp);
                es.wi();
                es.w("return " + tmp + ";");
                flushTrailing(es, st.line);
                es.nl();
                break;
            }
            es.w("return");
            if (st.e) {
                TypeInfo expect;
                if (es.hasCurRet && !es.curRet.isVar && !es.curRet.empty() &&
                    es.curRet.name != "void") {
                    expect = toInfo(es.ctx, es.curRet);
                }
                if (st.e->k == EK::Lit && st.e->lt == TT::Null && expect.known &&
                    es.typeParams.count(expect.name) > 0) {
                    es.w(" default");
                } else {
                    es.w(" ");
                    emitExpr(es, *st.e, 0, expect);
                }
            }
            es.w(";");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Throw:
            flushComments(es, st.line);
            es.wi();
            es.w("throw ");
            emitExpr(es, *st.e, 0);
            es.w(";");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Try:
            emitTry(es, st);
            break;
        case SK::Switch:
            emitSwitch(es, st);
            break;
        case SK::Sync:
            flushComments(es, st.line);
            es.wi();
            es.w("lock (" + captureExpr(es, *st.e, 0) + ")");
            es.nl();
            emitBodyStmt(es, *st.body);
            break;
        case SK::Assert:
            flushComments(es, st.line);
            es.ctx.needDiag = true;
            es.wi();
            es.w("Debug.Assert(" + captureExpr(es, *st.e, 0));
            if (st.e2) es.w(", " + captureExpr(es, *st.e2, 0));
            es.w(");");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Label: {
            flushComments(es, st.line);
            bool isLoop = st.body && (st.body->k == SK::While || st.body->k == SK::DoWhile ||
                                      st.body->k == SK::For || st.body->k == SK::Foreach);
            es.wi();
            es.w(csIdent(st.label) + ":;");
            flushTrailing(es, st.line);
            es.nl();
            if (st.body) {
                if (isLoop) {
                    es.activeLabels.insert(st.label);
                    Stmt* loop = st.body.get();
                    if (!loop->body || loop->body->k != SK::Block) {
                        auto wrapped = std::make_unique<Stmt>();
                        wrapped->k = SK::Block;
                        wrapped->line = loop->line;
                        wrapped->block.push_back(std::move(loop->body));
                        loop->body = std::move(wrapped);
                    }
                    auto contLabel = std::make_unique<Stmt>();
                    contLabel->k = SK::Label;
                    contLabel->label = st.label + "__cont";
                    contLabel->line = st.line;
                    loop->body->block.push_back(std::move(contLabel));
                    emitStmt(es, *loop);
                    es.activeLabels.erase(st.label);
                } else if (st.body->k == SK::Block) {
                    es.activeLabels.insert(st.label);
                    es.line("{");
                    es.indent++;
                    for (auto& s : st.body->block) emitStmt(es, *s);
                    es.indent--;
                    es.line("}");
                    es.activeLabels.erase(st.label);
                } else {
                    es.activeLabels.insert(st.label);
                    emitStmt(es, *st.body);
                    es.activeLabels.erase(st.label);
                }
                es.wi();
                es.w(csIdent(st.label) + "__end: ;");
                es.nl();
            }
            break;
        }
    }
}

bool switchENeedsStmtForm(Expr& e) {
    if (e.k != EK::SwitchE) return false;
    for (auto& arm : e.cases) {
        if (!arm.isBlockVal) continue;
        bool onlyYield = arm.body.size() == 1 && arm.body[0]->k == SK::Return && arm.body[0]->isYield;
        if (!onlyYield) return true;
    }
    return false;
}

void emitSwitchEAsStmt(ES& es, Expr& sw, const std::string& target) {
    auto stmt = std::make_unique<Stmt>();
    stmt->k = SK::Switch;
    stmt->line = sw.line;
    stmt->subject = std::move(sw.a);
    stmt->arrowSw = true;
    for (auto& arm : sw.cases) {
        CaseArm out;
        out.line = arm.line;
        out.isDefault = arm.isDefault;
        out.nullLabel = arm.nullLabel;
        out.labels = std::move(arm.labels);
        out.patLabels = std::move(arm.patLabels);
        out.patName = arm.patName;
        out.guard = std::move(arm.guard);
        if (arm.value) {
            auto as = std::make_unique<Stmt>();
            as->k = SK::ExprStmt;
            as->line = arm.line;
            auto tgt = std::make_unique<Expr>();
            tgt->k = EK::Ident;
            tgt->name = target;
            auto assign = std::make_unique<Expr>();
            assign->k = EK::Assign;
            assign->op = "=";
            assign->a = std::move(tgt);
            assign->b = std::move(arm.value);
            as->e = std::move(assign);
            out.body.push_back(std::move(as));
        } else {
            for (auto& s : arm.body) {
                if (s->k == SK::Return && s->isYield && s->e) {
                    auto as = std::make_unique<Stmt>();
                    as->k = SK::ExprStmt;
                    as->line = s->line;
                    auto tgt = std::make_unique<Expr>();
                    tgt->k = EK::Ident;
                    tgt->name = target;
                    auto assign = std::make_unique<Expr>();
                    assign->k = EK::Assign;
                    assign->op = "=";
                    assign->a = std::move(tgt);
                    assign->b = std::move(s->e);
                    as->e = std::move(assign);
                    out.body.push_back(std::move(as));
                } else {
                    out.body.push_back(std::move(s));
                }
            }
        }
        stmt->arms.push_back(std::move(out));
    }
    emitStmt(es, *stmt);
}

void emitLocalVar(ES& es, Stmt& st) {
    flushComments(es, st.line);
    bool useVar = st.ty.isVar;
    std::string csT = csTypeStr(es.ctx, st.ty);
    for (size_t i = 0; i < st.names.size(); i++) {
        std::string t = csT;
        for (int k = 0; k < st.dims[i]; k++) t += "[]";
        if (useVar && st.inits[i]) {
            TypeInfo it = inferExprType(es, *st.inits[i]);
            if (it.known && it.dims == 0 && st.dims[i] == 0 && !it.name.empty() &&
                it.name != "object" && it.name != "null") {
                t = it.text();
                useVar = false;
            }
        }
        es.wi();
        if (st.inits[i] && st.inits[i]->k == EK::SwitchE && switchENeedsStmtForm(*st.inits[i])) {
            es.w(t + " " + csIdent(st.names[i]) + ";");
            flushTrailing(es, st.line);
            es.nl();
            if (!useVar && !st.ty.isVar && !st.ty.name.empty()) {
                TypeInfo vt = toInfo(es.ctx, st.ty);
                vt.dims += st.dims[i];
                es.scope.push_back({st.names[i], vt});
            } else {
                es.scope.push_back({st.names[i], {}});
            }
            emitSwitchEAsStmt(es, *st.inits[i], st.names[i]);
            continue;
        }
        if (st.inits[i] && st.inits[i]->k == EK::Lit && st.inits[i]->lt == TT::Null &&
            es.typeParams.count(st.ty.name) > 0) {
            es.w(t + " " + csIdent(st.names[i]) + " = default;");
            flushTrailing(es, st.line);
            es.nl();
            es.scope.push_back({st.names[i], {}});
            continue;
        }
        es.w(t + " " + csIdent(st.names[i]));
        if (st.inits[i]) {
            es.w(" = ");
            TypeInfo expect;
            if (!useVar) {
                expect = toInfo(es.ctx, st.ty);
                expect.dims += st.dims[i];
                expect.known = !st.ty.isVar && !st.ty.name.empty();
            }
            emitExpr(es, *st.inits[i], 0, expect);
        }
        es.w(";");
        flushTrailing(es, st.line);
        es.nl();
        if (!useVar && !st.ty.isVar && !st.ty.name.empty()) {
            TypeInfo vt = toInfo(es.ctx, st.ty);
            vt.dims += st.dims[i];
            es.scope.push_back({st.names[i], vt});
        } else if (st.inits[i]) {
            TypeInfo it = inferExprType(es, *st.inits[i]);
            if (it.known) es.scope.push_back({st.names[i], it});
            else es.scope.push_back({st.names[i], {}});
        } else {
            es.scope.push_back({st.names[i], {}});
        }
    }
}

void emitFor(ES& es, Stmt& st) {
    flushComments(es, st.line);
    es.wi();
    es.w("for (");
    std::vector<VarInfo> pushed;
    if (!st.names.empty()) {
        std::string t = csTypeStr(es.ctx, st.ty);
        es.w(t + " ");
        for (size_t i = 0; i < st.names.size(); i++) {
            if (i) es.w(", ");
            std::string vt = t;
            for (int k = 0; k < st.dims[i]; k++) vt += "[]";
            es.w(csIdent(st.names[i]));
            TypeInfo vi = toInfo(es.ctx, st.ty);
            vi.dims += st.dims[i];
            pushed.push_back({st.names[i], vi});
            if (st.inits[i]) {
                es.w(" = ");
                TypeInfo expect;
                expect = vi;
                emitExpr(es, *st.inits[i], 0, expect);
            }
        }
    } else {
        for (size_t i = 0; i < st.initExprs.size(); i++) {
            if (i) es.w(", ");
            emitExpr(es, *st.initExprs[i], 0);
        }
    }
    es.w(";");
    if (st.e) es.w(" " + captureExpr(es, *st.e, 0));
    es.w(";");
    for (size_t i = 0; i < st.updateExprs.size(); i++) {
        es.w(i == 0 ? " " : ", ");
        emitExpr(es, *st.updateExprs[i], 0);
    }
    es.w(")");
    flushTrailing(es, st.line);
    es.nl();
    for (auto& v : pushed) es.scope.push_back(v);
    emitBodyStmt(es, *st.body);
    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
}

void emitForeach(ES& es, Stmt& st) {
    flushComments(es, st.line);
    std::string t = csTypeStr(es.ctx, st.ty);
    es.wi();
    es.w("foreach (" + t + " " + csIdent(st.names[0]) + " in " + captureExpr(es, *st.e, 0) + ")");
    flushTrailing(es, st.line);
    es.nl();
    es.scope.push_back({st.names[0], toInfo(es.ctx, st.ty)});
    emitBodyStmt(es, *st.body);
    es.scope.pop_back();
}

void chainInvoke(ES& es, const std::string& tmp, const std::string& vT, Expr& e) {
    std::string m = csIdent(e.name);
    if (e.name == "add" && (vT.rfind("List<", 0) == 0 || vT.rfind("LinkedList<", 0) == 0 ||
                            vT.rfind("IList<", 0) == 0 || vT.rfind("ICollection<", 0) == 0))
        m = "Add";
    else if (e.name == "put" && vT.rfind("Dictionary<", 0) == 0)
        m = "";
    if (!m.empty()) {
        es.w(csIdent(tmp) + "." + m + "(");
        emitArgs(es, e.args);
        es.w(");");
        es.nl();
    }
    if (m.empty()) {
        es.wi();
        es.w(csIdent(tmp) + "[" + captureExprText(es, *e.args[0], 0) + "] = " +
             captureExprText(es, *e.args[1], 0) + ";");
        es.nl();
    }
}

bool emitDictModify(ES& es, Expr& e, const std::string& recv, const TypeInfo& rt,
                    const std::string* chainTmp) {
    if (e.name == "merge" && e.args.size() == 3) {
        std::string tmp = chainTmp ? *chainTmp : "__m" + std::to_string(es.tmpCounter++);
        std::string vT = rt.known && rt.args.size() == 2 ? rt.args[1].text() : "object";
        Expr* fn = e.args[2].get();
        if (chainTmp) es.w(vT + " " + tmp + ";"), es.nl(), es.wi();
        if (fn->k == EK::MRef && fn->obj && fn->obj->k == EK::Ident) {
            std::string wc = mapJavaTypeFqn(fn->obj->name, true);
            std::string wcc = wc.empty() ? fn->obj->name : wc;
            if ((wcc == "int" || wcc == "long" || wcc == "double" || wcc == "float") &&
                (fn->name == "sum" || fn->name == "max" || fn->name == "min")) {
                std::unique_ptr<Expr> val;
                if (fn->name == "sum") {
                    val = std::make_unique<Expr>();
                    val->k = EK::Bin;
                    val->op = "+";
                    val->a = mkIdent(tmp);
                    val->b = cloneExpr(*e.args[1]);
                } else {
                    val = std::make_unique<Expr>();
                    val->k = EK::Call;
                    val->name = fn->name == "max" ? "Max" : "Min";
                    auto mo = std::make_unique<Expr>();
                    mo->k = EK::Ident;
                    mo->name = "Math";
                    val->obj = std::move(mo);
                    val->args.push_back(mkIdent(tmp));
                    val->args.push_back(cloneExpr(*e.args[1]));
                }
                es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
                     ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
                es.nl();
                es.wi();
                es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = ");
                emitExpr(es, *val, 0);
                es.w(";");
                es.nl();
                es.wi();
                es.w("else");
                es.nl();
                es.wi();
                es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
                     captureExpr(es, *e.args[1], 20) + ";");
                es.nl();
                return true;
            }
        }
        if (fn->k == EK::Lambda && !fn->lbodyIsBlock && fn->lparams.size() == 2 &&
            fn->lbody) {
            std::unique_ptr<Expr> val = cloneExpr(*fn->lbody);
            Expr oldVal;
            oldVal.k = EK::Ident;
            oldVal.name = tmp;
            substParamExpr(*val, fn->lparams[0].second, oldVal);
            substParamExpr(*val, fn->lparams[1].second, *e.args[1]);
            es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
                 ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
            es.nl();
            es.wi();
            es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = ");
            emitExpr(es, *val, 0);
            es.w(";");
            es.nl();
            es.wi();
            es.w("else");
            es.nl();
            es.wi();
            es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
                 captureExpr(es, *e.args[1], 20) + ";");
            es.nl();
            return true;
        }
        es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
             ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[2], 20) + "(" + tmp + ", " +
             captureExpr(es, *e.args[1], 20) + ");");
        es.nl();
        es.wi();
        es.w("else");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[1], 20) + ";");
        es.nl();
        return true;
    }
    if (e.name == "computeIfAbsent" && e.args.size() == 2) {
        std::string tmp = chainTmp ? *chainTmp : "__c" + std::to_string(es.tmpCounter++);
        std::string vT = rt.known && rt.args.size() == 2 ? rt.args[1].text() : "object";
        Expr* fn = e.args[1].get();
        if (chainTmp) es.w(vT + " " + tmp + ";"), es.nl(), es.wi();
        if (fn->k == EK::Lambda && !fn->lbodyIsBlock && fn->lparams.size() == 1 &&
            fn->lbody) {
            std::unique_ptr<Expr> val = cloneExpr(*fn->lbody);
            substParamExpr(*val, fn->lparams[0].second, *e.args[0]);
            es.w("if (!" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
                 ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
            es.nl();
            es.wi();
            es.w("{");
            es.nl();
            es.wi();
            es.w("    " + tmp + " = ");
            TypeInfo vti;
            vti.known = true;
            vti.name = vT;
            emitExpr(es, *val, 0, vti);
            es.w(";");
            es.nl();
            es.wi();
            es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " + tmp + ";");
            es.nl();
            es.wi();
            es.w("}");
            es.nl();
            return true;
        }
        es.w("if (!" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
             ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
        es.nl();
        es.wi();
        es.w("{");
        es.nl();
        es.wi();
        std::string kT = rt.known && rt.args.size() == 2 ? rt.args[0].text() : "object";
        es.w("    " + tmp + " = ((System.Func<" + kT + ", " + vT + ">)(" +
             captureExpr(es, *e.args[1], 20) + "))(" + captureExpr(es, *e.args[0], 20) + ");");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " + tmp + ";");
        es.nl();
        es.wi();
        es.w("}");
        es.nl();
        return true;
    }
    if (e.name == "compute" && e.args.size() == 2) {
        std::string tmp = chainTmp ? *chainTmp : "__k" + std::to_string(es.tmpCounter++);
        std::string vT = rt.known && rt.args.size() == 2 ? rt.args[1].text() : "object";
        if (chainTmp) es.w(vT + " " + tmp + ";"), es.nl(), es.wi();
        es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
             ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[1], 20) + "(" + captureExpr(es, *e.args[0], 20) +
             ", " + tmp + ");");
        es.nl();
        es.wi();
        es.w("else");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[1], 20) + "(" + captureExpr(es, *e.args[0], 20) +
             ", null);");
        es.nl();
        return true;
    }
    return false;
}

bool emitExprStmt(ES& es, Expr& e) {
    if (e.k == EK::Call && e.obj) {
        if (e.obj->k == EK::Call && e.obj->obj) {
            Expr& inner = *e.obj;
            TypeInfo irt = inferExprType(es, *inner.obj);
            if (irt.known && (irt.name == "Dictionary" || irt.name == "SortedDictionary")) {
                std::string recvI = captureExpr(es, *inner.obj, 20);
                std::string tmp = "__t" + std::to_string(es.tmpCounter++);
                if (emitDictModify(es, inner, recvI, irt, &tmp)) {
                    es.wi();
                    chainInvoke(es, tmp, irt.known && irt.args.size() == 2 ? irt.args[1].text() : "object", e);
                    return true;
                }
            }
        }
        TypeInfo rt = inferExprType(es, *e.obj);
        std::string recv = captureExpr(es, *e.obj, 20);
        if (rt.known && (rt.name == "Dictionary" || rt.name == "SortedDictionary")) {
            if (emitDictModify(es, e, recv, rt, nullptr)) return true;
            if (e.name == "putAll" && e.args.size() == 1) {
                std::string tmp = "__p" + std::to_string(es.tmpCounter++);
                es.w("foreach (var " + tmp + " in " + captureExpr(es, *e.args[0], 20) + ")");
                es.nl();
                es.wi();
                es.w("    " + recv + "[" + tmp + ".Key] = " + tmp + ".Value;");
                es.nl();
                return true;
            }
        }
        if (rt.known && rt.name == "StringBuilder") {
            if (e.name == "reverse" && e.args.empty()) {
                es.ctx.needLinq = true;
                es.w(recv + " = new string(" + recv + ".ToString().Reverse().ToArray());");
                es.nl();
                return true;
            }
        }
        if (rt.known && rt.name == "__Scanner") {
            if (e.name == "close" && e.args.empty()) return true;
        }
    }
    emitExpr(es, e, 0);
    return false;
}
const std::set<std::string>& staticReceiverNames() {
    static const std::set<std::string> s = {
        "Math", "StrictMath", "System", "Integer", "Long", "Short", "Byte", "Double",
        "Float", "Boolean", "Character", "String", "Objects", "Arrays", "Collections",
        "Thread", "Optional", "UUID", "Instant", "LocalDate", "LocalTime", "LocalDateTime",
        "Duration", "Period", "Pattern", "Files", "Paths", "Locale", "BigDecimal",
        "BigInteger", "Stream", "IntStream", "LongStream", "DoubleStream", "Collectors",
        "Enum", "Environment", "Console"};
    return s;
}

std::string pathText(const Expr& e) {
    if (e.k == EK::Ident) return e.name;
    if (e.k == EK::Field && e.obj) {
        std::string base = pathText(*e.obj);
        if (base == "__special__") return base;
        return base + "." + e.name;
    }
    return "__special__";
}

bool isStringLit(Expr& e) { return e.k == EK::Lit && (e.lt == TT::Str || e.lt == TT::TBlk); }

std::string stripNullable(const std::string& t) {
    if (endsWith(t, "?")) return t.substr(0, t.size() - 1);
    return t;
}

TypeInfo elemTypeOf(const TypeInfo& t) { return t.elem(); }

TypeInfo inferExprType(ES& es, Expr& e);

TypeInfo inferArgsNone() { return TypeInfo(); }

TypeInfo inferExprType(ES& es, Expr& e) {
    switch (e.k) {
        case EK::Lit: {
            TypeInfo t;
            t.known = true;
            if (e.lt == TT::Str || e.lt == TT::TBlk) t.name = "string";
            else if (e.lt == TT::Chr) t.name = "char";
            else if (e.lt == TT::Bool) t.name = "bool";
            else if (e.lt == TT::Null) t.name = "null";
            else if (e.lt == TT::Int) {
                if (endsWith(e.val, "L")) t.name = "long";
                else t.name = "int";
            } else if (e.lt == TT::Flt) {
                char last = e.val.empty() ? 'd' : e.val.back();
                if (last == 'f' || last == 'F') t.name = "float";
                else t.name = "double";
            }
            return t;
        }
        case EK::Ident: {
            std::string base = firstSeg(e.name);
            for (size_t i = es.scope.size(); i > 0; i--) {
                if (es.scope[i - 1].name == base) {
                    TypeInfo r = es.scope[i - 1].ty;
                    if (e.name.size() > base.size()) r.known = false;
                    return r;
                }
            }
            auto ft = es.ctx.classFieldType.find(es.cls + "." + base);
            if (ft != es.ctx.classFieldType.end()) {
                TypeInfo r = toInfo(es.ctx, ft->second);
                if (e.name.size() > base.size()) r.known = false;
                return r;
            }
            std::string anc = ancestorOf(es.ctx, es.cls);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                auto ft2 = es.ctx.classFieldType.find(anc + "." + base);
                if (ft2 != es.ctx.classFieldType.end()) {
                    TypeInfo r = toInfo(es.ctx, ft2->second);
                    if (e.name.size() > base.size()) r.known = false;
                    return r;
                }
                anc = ancestorOf(es.ctx, anc);
            }
            if (es.ctx.enumConstOwner.count(base)) {
                TypeInfo t;
                t.known = true;
                t.name = es.ctx.enumConstOwner[base];
                return t;
            }
            if (es.ctx.staticImports.count(e.name)) return TypeInfo();
            if (staticReceiverNames().count(base) || es.ctx.typeByName.count(base)) {
                if (e.name == base) {
                    TypeInfo t;
                    t.known = true;
                    t.name = base;
                    return t;
                }
            }
            if (base == "System" && e.name == "System.out") {
                TypeInfo t;
                t.known = true;
                t.name = "__consoleOut";
                return t;
            }
            if (base == "System" && e.name == "System.err") {
                TypeInfo t;
                t.known = true;
                t.name = "__consoleErr";
                return t;
            }
            return TypeInfo();
        }
        case EK::This: {
            TypeInfo t;
            t.known = true;
            t.name = es.cls;
            return t;
        }
        case EK::Field: {
            if (!e.obj) return TypeInfo();
            if (e.name == "this") {
                TypeInfo t;
                t.known = true;
                t.name = firstSeg(pathText(*e.obj));
                return t;
            }
            TypeInfo ot = inferExprType(es, *e.obj);
            if (!ot.known) return TypeInfo();
            if (ot.dims > 0 && e.name == "length") {
                TypeInfo t;
                t.known = true;
                t.name = "int";
                return t;
            }
            if (ot.name == "string" && e.name == "Length") {
                TypeInfo t;
                t.known = true;
                t.name = "int";
                return t;
            }
            if (ot.name == "Math" && (e.name == "PI" || e.name == "E")) {
                TypeInfo t;
                t.known = true;
                t.name = "double";
                return t;
            }
            if (isNumericCs(stripNullable(ot.name)) && endsWith(ot.name, "?")) {
                if (e.name == "Value" || e.name == "HasValue") {
                    TypeInfo t;
                    t.known = true;
                    t.name = e.name == "Value" ? stripNullable(ot.name) : "bool";
                    return t;
                }
            }
            if (ot.name == "DateTimeOffset") {
                if (e.name == "ToUnixTimeMilliseconds" || e.name == "ToUnixTimeSeconds") return TypeInfo();
            }
            auto ft = es.ctx.classFieldType.find(ot.name + "." + e.name);
            if (ft != es.ctx.classFieldType.end()) return toInfo(es.ctx, ft->second);
            std::string anc = ancestorOf(es.ctx, ot.name);
            int guard2 = 0;
            while (!anc.empty() && guard2++ < 32) {
                auto ft2 = es.ctx.classFieldType.find(anc + "." + e.name);
                if (ft2 != es.ctx.classFieldType.end()) return toInfo(es.ctx, ft2->second);
                anc = ancestorOf(es.ctx, anc);
            }
            if (es.ctx.enumClassTypes.count(ot.name)) {
                if (e.name == "Ordinal") {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "Name") {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
            }
            return TypeInfo();
        }
        case EK::Call: {
            TypeInfo ot;
            if (e.obj) ot = inferExprType(es, *e.obj);
            std::string staticOwner = e.obj ? lastSeg(pathText(*e.obj)) : "";
            if (e.obj && (e.obj->k == EK::Ident || e.obj->k == EK::Field)) {
                if (staticOwner == "List" || staticOwner == "ArrayList") {
                    if (e.name == "of") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "List";
                        if (!e.args.empty()) {
                            TypeInfo element = inferExprType(es, *e.args.front());
                            if (element.known) t.args.push_back(element);
                        }
                        return t;
                    }
                }
                if (staticOwner == "Set" || staticOwner == "HashSet") {
                    if (e.name == "of") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "HashSet";
                        if (!e.args.empty()) {
                            TypeInfo element = inferExprType(es, *e.args.front());
                            if (element.known) t.args.push_back(element);
                        }
                        return t;
                    }
                }
                if (staticOwner == "Thread") {
                    if (e.name == "currentThread") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "Thread";
                        return t;
                    }
                }
                if (staticOwner == "Map" || staticOwner == "HashMap") {
                    if (e.name == "of") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "Dictionary";
                        if (e.args.size() >= 2) {
                            TypeInfo key = inferExprType(es, *e.args[0]);
                            TypeInfo value = inferExprType(es, *e.args[1]);
                            if (key.known) t.args.push_back(key);
                            if (value.known) t.args.push_back(value);
                        }
                        return t;
                    }
                }
                if (staticOwner == "Stream" || staticOwner == "IntStream" ||
                    staticOwner == "LongStream" || staticOwner == "DoubleStream") {
                    if (e.name == "of" || e.name == "empty" || e.name == "range" ||
                        e.name == "rangeClosed") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "IEnumerable";
                        TypeInfo element;
                        if (staticOwner == "IntStream") {
                            element.known = true;
                            element.name = "int";
                        } else if (staticOwner == "LongStream") {
                            element.known = true;
                            element.name = "long";
                        } else if (staticOwner == "DoubleStream") {
                            element.known = true;
                            element.name = "double";
                        } else if (!e.targs.empty()) {
                            element = toInfo(es.ctx, e.targs.front());
                        } else if (!e.args.empty()) {
                            element = inferExprType(es, *e.args.front());
                        }
                        if (!element.known) {
                            element.known = true;
                            element.name = "object";
                        }
                        t.args.push_back(element);
                        return t;
                    }
                }
            }
            if (e.name == "stream") {
                TypeInfo t = ot;
                if (t.known) t.name = "IEnumerable";
                return t;
            }
            if (e.name == "boxed") {
                TypeInfo t = ot;
                if (t.known) t.name = "IEnumerable";
                return t;
            }
            if (e.name == "entrySet" && ot.known &&
                (ot.name == "Dictionary" || ot.name == "SortedDictionary")) {
                TypeInfo t = ot;
                t.name = "IEnumerable";
                t.args.clear();
                return t;
            }
            if (ot.known) {
                std::string base = stripNullable(ot.name);
                auto& rets = es.ctx.methodReturns[base];
                auto it = rets.find(e.name + "/" + std::to_string(e.args.size()));
                if (it != rets.end() && !it->second.empty()) return toInfo(es.ctx, it->second);
                std::string anc = ancestorOf(es.ctx, base);
                int guard = 0;
                while (!anc.empty() && guard++ < 32) {
                    auto& rets2 = es.ctx.methodReturns[anc];
                    auto it2 = rets2.find(e.name + "/" + std::to_string(e.args.size()));
                    if (it2 != rets2.end() && !it2->second.empty())
                        return toInfo(es.ctx, it2->second);
                    anc = ancestorOf(es.ctx, anc);
                }
                if (e.name == "get" && ot.args.size() == 1 &&
                    (ot.name == "List" || ot.name == "IList" || ot.name == "LinkedList"))
                    return ot.args[0];
                if (e.name == "get" && ot.args.size() == 2 &&
                    (ot.name == "Dictionary" || ot.name == "SortedDictionary"))
                    return ot.args[1];
                if (e.name == "Values" && es.ctx.enumClassTypes.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = ot.name;
                    t.dims = 1;
                    return t;
                }
                if (e.name == "values" &&
                    (es.ctx.simpleEnums.count(ot.name) || es.ctx.enumClassTypes.count(ot.name))) {
                    TypeInfo t = ot;
                    t.dims = 1;
                    return t;
                }
                if (e.name == "keySet" && ot.args.size() == 2) return ot.args[0];
                if (e.name == "values" && ot.args.size() == 2) return ot.args[1];
                if (e.name == "entrySet" && ot.args.size() == 2) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "IEnumerable";
                    return t;
                }
                if (e.name == "getOrDefault" && ot.args.size() == 2) return ot.args[1];
                if (e.name == "toString" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "getClass" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "Type";
                    return t;
                }
                if (e.name == "ordinal" && e.args.empty() && es.ctx.simpleEnums.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "ordinal" && e.args.empty() && es.ctx.enumClassTypes.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "name" && e.args.empty() && es.ctx.simpleEnums.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "name" && e.args.empty() && es.ctx.enumClassTypes.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "doubleValue" || e.name == "floatValue") {
                    TypeInfo t;
                    t.known = true;
                    t.name = endsWith(ot.name, "?") ? stripNullable(ot.name) : ot.name;
                    return t;
                }
                if (endsWith(ot.name, "?") && e.name == "Value") {
                    TypeInfo t;
                    t.known = true;
                    t.name = stripNullable(ot.name);
                    return t;
                }
                if (e.name == "hasNext" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "bool";
                    return t;
                }
                if (e.name == "next" && e.args.empty() && !ot.args.empty()) return ot.args[0];
                if (e.name == "Current" && !ot.args.empty()) return ot.args[0];
                if (e.name == "append" && ot.name == "StringBuilder") {
                    TypeInfo t;
                    t.known = true;
                    t.name = "StringBuilder";
                    return t;
                }
                if (e.name == "ToString" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "getMessage" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "orElse" && e.args.size() == 1 && endsWith(ot.name, "?")) {
                    TypeInfo t;
                    t.known = true;
                    t.name = stripNullable(ot.name);
                    t.args = ot.args;
                    return t;
                }
                static const std::set<std::string> linqKeep = {
                    "Where", "Select", "SelectMany", "OrderBy", "Take", "Skip",
                    "Distinct", "Concat", "Reverse", "OfType", "Cast",
                    "filter", "map", "mapToInt", "mapToLong", "mapToDouble",
                    "mapToObj", "flatMap", "distinct", "sorted", "limit", "skip",
                    "peek"};
                if (linqKeep.count(e.name) && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "IEnumerable";
                    t.args = ot.args;
                    if ((e.name == "map" || e.name == "mapToInt" || e.name == "mapToLong" ||
                         e.name == "mapToDouble" || e.name == "mapToObj") && !e.args.empty()) {
                        TypeInfo mapped;
                        if (e.name == "mapToInt") {
                            mapped.known = true;
                            mapped.name = "int";
                        } else if (e.name == "mapToLong") {
                            mapped.known = true;
                            mapped.name = "long";
                        } else if (e.name == "mapToDouble") {
                            mapped.known = true;
                            mapped.name = "double";
                        } else if (e.args[0]->k == EK::Lambda && e.args[0]->lbody) {
                            mapped = inferExprType(es, *e.args[0]->lbody);
                        }
                        if (mapped.known) t.args[0] = mapped;
                    } else if (e.name == "flatMap" && !e.args.empty() &&
                               e.args[0]->k == EK::Lambda && e.args[0]->lbody) {
                        TypeInfo mapped = inferExprType(es, *e.args[0]->lbody);
                        if (mapped.known && mapped.name == "IEnumerable" && !mapped.args.empty())
                            t.args[0] = mapped.args[0];
                    }
                    return t;
                }
                if (e.name == "ToList" && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "List";
                    t.args = ot.args;
                    return t;
                }
                if (e.name == "ToHashSet" && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "HashSet";
                    t.args = ot.args;
                    return t;
                }
                if (e.name == "ToArray" && !ot.args.empty()) {
                    TypeInfo t = ot.args[0];
                    t.dims += 1;
                    return t;
                }
                if (e.name == "Count" && e.args.empty() && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "First" || e.name == "Last" || e.name == "FirstOrDefault" ||
                    e.name == "LastOrDefault" || e.name == "findFirst" || e.name == "findAny") {
                    if (!ot.args.empty()) {
                        TypeInfo t = ot.args[0];
                        if (!t.name.empty() && !endsWith(t.name, "?") && !t.name.empty() &&
                            t.name != "int" && t.name != "long" && t.name != "double" &&
                            t.name != "float" && t.name != "bool" && t.name != "char")
                            t.name += "?";
                        return t;
                    }
                }
            }
            if (!e.obj) {
                std::string base = firstSeg(e.name);
                auto& rets = es.ctx.methodReturns[es.cls];
                auto it = rets.find(e.name + "/" + std::to_string(e.args.size()));
                if (it != rets.end() && !it->second.empty()) return toInfo(es.ctx, it->second);
            }
            return TypeInfo();
        }
        case EK::New: {
            TypeInfo t = toInfo(es.ctx, e.ty);
            return t;
        }
        case EK::NewArr: {
            TypeInfo t = toInfo(es.ctx, e.ty);
            if (t.known) t.dims += (int)e.dims.size();
            return t;
        }
        case EK::ArrIdx: {
            if (!e.a) return TypeInfo();
            TypeInfo bt = inferExprType(es, *e.a);
            if (!bt.known) return TypeInfo();
            if (bt.dims > 0) {
                TypeInfo t = bt;
                t.dims--;
                return t;
            }
            if (!bt.args.empty()) {
                if ((bt.name == "Dictionary" || bt.name == "SortedDictionary") &&
                    bt.args.size() == 2)
                    return bt.args[1];
                return bt.args[0];
            }
            return TypeInfo();
        }
        case EK::Bin: {
            if (e.op == "&&" || e.op == "||" || e.op == "==" || e.op == "!=" || e.op == "<" ||
                e.op == ">" || e.op == "<=" || e.op == ">=") {
                TypeInfo t;
                t.known = true;
                t.name = "bool";
                return t;
            }
            TypeInfo a = e.a ? inferExprType(es, *e.a) : TypeInfo();
            TypeInfo b = e.b ? inferExprType(es, *e.b) : TypeInfo();
            if (a.known && a.name == "string") return a;
            if (b.known && b.name == "string") return b;
            if (e.op == "+" && a.known && (a.name == "string")) return a;
            if (a.known && b.known) {
                int ra = csRank(a.name), rb = csRank(b.name);
                if (ra > 0 && rb > 0) {
                    TypeInfo t;
                    t.known = true;
                    t.name = ra >= rb ? a.name : b.name;
                    if (ra == 3 && rb == 3 && (a.name == "char" || b.name == "char")) t.name = "int";
                    return t;
                }
            }
            return a.known ? a : b;
        }
        case EK::Un: {
            if (e.op == "!") {
                TypeInfo t;
                t.known = true;
                t.name = "bool";
                return t;
            }
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::Post: {
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::Assign: {
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::Ternary: {
            TypeInfo a = e.b ? inferExprType(es, *e.b) : TypeInfo();
            if (a.known) return a;
            return e.c ? inferExprType(es, *e.c) : TypeInfo();
        }
        case EK::Cast: {
            TypeInfo t = toInfo(es.ctx, e.ty);
            return t;
        }
        case EK::InstOf: {
            TypeInfo t;
            t.known = true;
            t.name = "bool";
            return t;
        }
        case EK::Paren: {
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::SwitchE: {
            for (auto& arm : e.cases) {
                if (arm.value) return inferExprType(es, *arm.value);
                if (arm.isBlockVal && !arm.body.empty()) {
                    for (auto& s : arm.body)
                        if (s->k == SK::Return && s->e) return inferExprType(es, *s->e);
                }
            }
            return TypeInfo();
        }
        case EK::CtorCall:
        case EK::Lambda:
        case EK::MRef:
        case EK::ClassLit:
        default:
            return TypeInfo();
    }
}
const int P_BITOR = 3, P_XOR = 4, P_BITAND = 5, P_EQ = 6, P_REL = 7,
          P_ADD = 9, P_MUL = 10, P_UNARY = 11, P_POST = 12, P_PRIMARY = 13;

void emitExpr(ES& es, Expr& e, int prec, const TypeInfo& expect);
void emitCall(ES& es, Expr& e, const TypeInfo& expect);

void emitArgs(ES& es, std::vector<std::unique_ptr<Expr>>& args) {
    for (size_t i = 0; i < args.size(); i++) {
        if (i) es.w(", ");
        emitExpr(es, *args[i], 0);
    }
}

void emitFmtArg(ES& es, Expr& e, int tag) {
    if (tag == 1) {
        TypeInfo at = inferExprType(es, e);
        if (at.known && at.name == "bool") {
            es.w("(");
            emitExpr(es, e, 0);
            es.w(" ? \"true\" : \"false\")");
            return;
        }
    } else if (tag == 2) {
        es.w("Convert.ToString(");
        emitExpr(es, e, 0);
        es.w(", 8)");
        return;
    }
    emitExpr(es, e, 0);
}

std::string captureExprText(ES& es, Expr& e, int prec, const TypeInfo& expect) {
    std::ostringstream sub;
    std::ostream* saved = es.cur;
    es.cur = &sub;
    emitExpr(es, e, prec, expect);
    es.cur = saved;
    return sub.str();
}

}
