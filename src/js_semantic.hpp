#pragma once
#include "js_parser.hpp"
namespace js {
inline std::string lastSeg(const std::string& dotted) {
    size_t dot = dotted.rfind('.');
    return dot == std::string::npos ? dotted : dotted.substr(dot + 1);
}

std::string firstSeg(const std::string& dotted) {
    size_t dot = dotted.find('.');
    return dot == std::string::npos ? dotted : dotted.substr(0, dot);
}

std::string ownerSimple(const std::string& owner) {
    size_t dot = owner.rfind('.');
    return dot == std::string::npos ? owner : owner.substr(dot + 1);
}

TypeInfo toInfo(Ctx& ctx, const TypeRef& t, bool genArg = false) {
    (void)ctx;
    TypeInfo ti;
    if (t.wildcard) {
        if (!t.name.empty()) {
            TypeRef b;
            b.name = t.name;
            b.args = t.args;
            ti = toInfo(ctx, b, genArg);
        }
        return ti;
    }
    if (t.name.empty() || t.isVar) return ti;
    if (t.name == "Optional" && t.args.size() == 1) {
        TypeInfo inner = toInfo(ctx, t.args[0], true);
        if (!inner.name.empty() && !endsWith(inner.name, "?")) inner.name += "?";
        inner.known = true;
        return inner;
    }
    std::string mapped = mapJavaTypeFqn(t.name, genArg);
    if (mapped == "__Scanner") ctx.needScanner = true;
    ti.name = mapped.empty() ? t.name : mapped;
    for (auto& a : t.args) ti.args.push_back(toInfo(ctx, a, true));
    ti.dims = t.dims;
    ti.known = true;
    return ti;
}

const std::set<std::string>& knownInterfaces() {
    static const std::set<std::string> s = {
        "Runnable", "Comparable", "Comparator", "Iterable", "Callable", "Cloneable",
        "Serializable", "AutoCloseable", "Closeable", "Flushable", "Readable",
        "Appendable", "ActionListener", "MouseListener", "KeyListener", "ItemListener",
        "WindowListener", "Externalizable", "EventListener"};
    return s;
}

bool isIfaceType(Ctx& ctx, const TypeRef& t) {
    if (t.name.empty()) return false;
    std::string simple = firstSeg(t.name);
    auto it = ctx.typeByName.find(simple);
    if (it != ctx.typeByName.end()) return it->second->dk == 1;
    std::string mapped = mapJavaTypeFqn(t.name, false);
    if (mapped == "Action" || mapped == "Func" || mapped == "Comparison" ||
        mapped == "Predicate" || mapped == "IEnumerable" || mapped == "IEnumerator" ||
        mapped == "IComparable" || mapped == "ICloneable" || mapped == "IDisposable")
        return true;
    if (knownInterfaces().count(simple)) return true;
    return false;
}

void registerDecl(Ctx& ctx, Decl& d) {
    if (!d.name.empty()) ctx.typeByName[d.name] = &d;
    for (auto& m : d.members)
        if (m.mk == 4 && m.nested) registerDecl(ctx, *m.nested);
}

void collectClassInfo(Ctx& ctx, Decl& d) {
    std::set<std::string>& fnames = ctx.classFieldNames[d.name];
    auto& rets = ctx.methodReturns[d.name];
    for (auto& m : d.members) {
        if (m.mk == 0) {
            for (size_t i = 0; i < m.names.size(); i++) {
                fnames.insert(m.names[i]);
                TypeRef t = m.ty;
                t.dims += m.fdims[i];
                ctx.classFieldType[d.name + "." + m.names[i]] = t;
                if (hasMod(m.mods, "static")) ctx.staticFields.insert(d.name + "." + m.names[i]);
                else ctx.instanceFields.insert(d.name + "." + m.names[i]);
            }
        } else if (m.mk == 1) {
            rets[m.name + "/" + std::to_string(m.params.size())] = m.ty;
            std::vector<TypeRef> ptypes;
            for (auto& p : m.params) ptypes.push_back(p.first);
            ctx.methodParamTypes[d.name + "." + m.name + "/" + std::to_string(m.params.size())] = ptypes;
            if (hasMod(m.mods, "static")) ctx.staticMethods.insert(d.name + "." + m.name + "/" + std::to_string(m.params.size()));
            else ctx.instanceMethods.insert(d.name + "." + m.name + "/" + std::to_string(m.params.size()));
        } else if (m.mk == 2) {
            std::vector<TypeRef> ptypes;
            for (auto& p : m.params) ptypes.push_back(p.first);
            ctx.methodParamTypes[d.name + ".<init>/" + std::to_string(m.params.size())] = ptypes;
        }
        if (m.mk == 4 && m.nested) collectClassInfo(ctx, *m.nested);
    }
}

std::string staticMathName(const std::string& m) {
    static const std::map<std::string, std::string> names = {
        {"abs", "Abs"}, {"max", "Max"}, {"min", "Min"}, {"pow", "Pow"}, {"sqrt", "Sqrt"},
        {"floor", "Floor"}, {"ceil", "Ceiling"}, {"round", "Round"}, {"log", "Log"},
        {"log10", "Log10"}, {"exp", "Exp"}, {"sin", "Sin"}, {"cos", "Cos"}, {"tan", "Tan"},
        {"asin", "Asin"}, {"acos", "Acos"}, {"atan", "Atan"}, {"atan2", "Atan2"},
        {"sinh", "Sinh"}, {"cosh", "Cosh"}, {"tanh", "Tanh"}, {"signum", "Sign"},
        {"hypot", "Hypot"}, {"cbrt", "Cbrt"}, {"toRadians", "ToRadians"},
        {"toDegrees", "ToDegrees"}, {"PI", "PI"}, {"E", "E"}};
    auto it = names.find(m);
    if (it != names.end()) return it->second;
    return m;
}

std::string arraysStaticName(const std::string& m) {
    static const std::map<std::string, std::string> names = {
        {"sort", "Sort"}, {"fill", "Fill"}, {"copyOf", "CopyOf"},
        {"copyOfRange", "CopyOfRange"}, {"toString", "ToString"},
        {"deepToString", "ToString"}, {"equals", "Equals"}, {"hashCode", "HashCode"},
        {"asList", "AsList"}, {"binarySearch", "BinarySearch"}};
    auto it = names.find(m);
    if (it != names.end()) return it->second;
    return m;
}

std::string wrapperConstCs(const std::string& ownerSimple, const std::string& member) {
    static const std::map<std::string, std::string> m = {
        {"MAX_VALUE", "MaxValue"}, {"MIN_VALUE", "MinValue"},
        {"POSITIVE_INFINITY", "PositiveInfinity"}, {"NEGATIVE_INFINITY", "NegativeInfinity"},
        {"NaN", "NaN"}};
    auto& wm = wrapperMap();
    auto wit = wm.find(ownerSimple);
    if (wit == wm.end()) return "";
    auto it = m.find(member);
    if (it == m.end()) return "";
    return mapPrim(wit->second) + "." + it->second;
}

void processImports(Ctx& ctx, Unit& u) {
    for (auto& imp : u.imports) {
        const std::string& path = imp.first;
        bool stat = imp.second;
        if (stat) {
            if (endsWith(path, ".*")) {
                std::string owner = path.substr(0, path.size() - 2);
                std::string simple = ownerSimple(owner);
                if (simple == "Math" && startsWith(owner, "java.lang")) ctx.needStaticMath = true;
                continue;
            }
            size_t dot = path.rfind('.');
            if (dot == std::string::npos) continue;
            std::string owner = path.substr(0, dot);
            std::string member = path.substr(dot + 1);
            std::string osimple = ownerSimple(owner);
            if ((osimple == "Math" || osimple == "StrictMath") && startsWith(owner, "java.lang")) {
                ctx.staticImports[member] = "Math." + staticMathName(member);
            } else if (osimple == "System" && startsWith(owner, "java.lang")) {
                if (member == "out") ctx.staticImports["out"] = "Console.Out";
                else if (member == "err") ctx.staticImports["err"] = "Console.Error";
                else if (member == "in") ctx.staticImports["in"] = "Console.In";
            } else if (osimple == "Objects" && startsWith(owner, "java.util")) {
                ctx.staticImports[member] = "Objects." + member;
            } else if (osimple == "Arrays" && startsWith(owner, "java.util")) {
                static const std::map<std::string, std::string> am = {
                    {"sort", "Array.Sort"}, {"fill", "Array.Fill"},
                    {"binarySearch", "Array.BinarySearch"},
                    {"asList", "__JHelp.AsList"}, {"toString", "__JHelp.ArrayToString"},
                    {"deepToString", "__JHelp.ArrayToString"}, {"equals", "__JHelp.ArrayEquals"},
                    {"copyOf", "__JHelp.CopyOf"}, {"copyOfRange", "__JHelp.CopyOfRange"},
                    {"hashCode", "Array.Empty<int>().GetHashCode"}};
                auto it = am.find(member);
                if (it != am.end()) ctx.staticImports[member] = it->second;
            } else if (osimple == "Collections" && startsWith(owner, "java.util")) {
                static const std::map<std::string, std::string> cm = {
                    {"sort", "__JHelp.Sort"}, {"reverse", "__JHelp.Reverse"},
                    {"max", "Enumerable.Max"}, {"min", "Enumerable.Min"},
                    {"swap", "__JHelp.Swap"}};
                auto it = cm.find(member);
                if (it != cm.end()) ctx.staticImports[member] = it->second;
            } else {
                std::string wc = wrapperConstCs(osimple, member);
                if (!wc.empty()) ctx.staticImports[member] = wc;
                else if (startsWith(owner, "java.")) {
                    continue;
                } else {
                    ctx.staticImports[member] = osimple + "." + member;
                }
            }
            continue;
        }
        std::string pkg = path;
        if (endsWith(pkg, ".*")) pkg = pkg.substr(0, pkg.size() - 2);
        else {
            size_t lastDot = pkg.rfind('.');
            if (lastDot != std::string::npos) pkg = pkg.substr(0, lastDot);
            else pkg = "";
        }
        if (pkg.empty()) continue;
        std::string csns = csNamespaceForJavaPkg(pkg);
        if (csns != pkg) {
            ctx.usings.insert(csns);
            continue;
        }
        if (startsWith(pkg, "java.") || startsWith(pkg, "javax.")) {
            size_t d1 = pkg.find('.', 5);
            std::string top2 = d1 == std::string::npos ? pkg : pkg.substr(0, d1);
            std::string mapped = csNamespaceForJavaPkg(top2);
            if (mapped != top2) ctx.usings.insert(mapped);
            continue;
        }
        ctx.usings.insert(pkg);
    }
}

std::string ancestorOf(Ctx& ctx, const std::string& cls) {
    auto it = ctx.typeByName.find(cls);
    if (it == ctx.typeByName.end()) return "";
    Decl* d = it->second;
    if (d->extends.name.empty()) return "";
    std::string base = firstSeg(d->extends.name);
    if (ctx.typeByName.count(base)) return base;
    return "";
}

bool isSubclassOf(Ctx& ctx, const std::string& cls, const std::string& base) {
    std::string c = cls;
    int guard = 0;
    while (!c.empty() && guard++ < 32) {
        if (c == base) return true;
        c = ancestorOf(ctx, c);
    }
    return false;
}

void resolveOverrides(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) {
        if (d->dk == 2) continue;
        for (auto& m : d->members) {
            if (m.mk != 1 || hasMod(m.mods, "static") || hasMod(m.mods, "private")) continue;
            std::string anc = ancestorOf(ctx, d->name);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                Decl* ad = ctx.typeByName[anc];
                bool found = false;
                for (auto& am : ad->members) {
                    if (am.mk != 1) continue;
                    if (am.name == m.name && am.params.size() == m.params.size() &&
                        !hasMod(am.mods, "static") && !hasMod(am.mods, "private")) {
                        ctx.virtualM[anc].insert(m.name + "/" + std::to_string(m.params.size()));
                        ctx.overrideM[d->name].insert(m.name + "/" + std::to_string(m.params.size()));
                        found = true;
                        break;
                    }
                }
                if (found) break;
                anc = ancestorOf(ctx, anc);
            }
        }
        for (auto& m : d->members)
            if (m.mk == 4 && m.nested) {
                Unit tmp;
                tmp.types.push_back(std::unique_ptr<Decl>(m.nested.get()));
                resolveOverrides(ctx, tmp);
                tmp.types[0].release();
            }
    }
}

void detectSamDelegatesDecl(Ctx& ctx, Decl& d, std::set<std::string>& candidates);
void collectDeclsRecursive(Decl& d, std::vector<Decl*>& out);
void scanAnonBasesExpr(const Expr& e, std::set<std::string>& out);
void scanAnonBasesStmt(const Stmt& st, std::set<std::string>& out);
void scanAnonBasesDeclBody(Decl& d, std::set<std::string>& out);
bool refsCaptureOrThis(const Expr& e, const std::set<std::string>& names);
bool refsCaptureOrThisStmt(const Stmt& st, const std::set<std::string>& names);
void appendFieldInitsToCtor(Member& ctor, const std::vector<std::pair<std::string, std::unique_ptr<Expr>>>& inits,
                            const std::set<std::string>& capNames);
std::unique_ptr<Expr> cloneExpr(const Expr& e);

void scanAnonBasesStmt(const Stmt& st, std::set<std::string>& out) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& x : st.inits)
                if (x) scanAnonBasesExpr(*x, out);
            break;
        case SK::Block:
            for (auto& s : st.block) scanAnonBasesStmt(*s, out);
            break;
        case SK::ExprStmt:
        case SK::Return:
        case SK::Throw:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            break;
        case SK::If:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            if (st.body2) scanAnonBasesStmt(*st.body2, out);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::For:
            for (auto& x : st.inits)
                if (x) scanAnonBasesExpr(*x, out);
            for (auto& x : st.initExprs)
                if (x) scanAnonBasesExpr(*x, out);
            for (auto& x : st.updateExprs)
                if (x) scanAnonBasesExpr(*x, out);
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::Foreach:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::Try:
            for (auto& x : st.resInits)
                if (x) scanAnonBasesExpr(*x, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            for (auto& c : st.catches) scanAnonBasesStmt(*c.body, out);
            if (st.fin) scanAnonBasesStmt(*st.fin, out);
            break;
        case SK::Switch:
            if (st.subject) scanAnonBasesExpr(*st.subject, out);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) scanAnonBasesExpr(*l, out);
                if (arm.guard) scanAnonBasesExpr(*arm.guard, out);
                if (arm.value) scanAnonBasesExpr(*arm.value, out);
                for (auto& s : arm.body) scanAnonBasesStmt(*s, out);
            }
            break;
        case SK::Sync:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::Assert:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.e2) scanAnonBasesExpr(*st.e2, out);
            break;
        case SK::Label:
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::LocalClass:
            if (st.lclass) scanAnonBasesDeclBody(*st.lclass, out);
            break;
        default:
            break;
    }
}

void scanAnonBasesDeclBody(Decl& d, std::set<std::string>& out);

void scanAnonBasesExpr(const Expr& e, std::set<std::string>& out) {
    if (e.k == EK::New && e.anon && !e.ty.name.empty()) out.insert(firstSeg(e.ty.name));
    if (e.obj) scanAnonBasesExpr(*e.obj, out);
    if (e.a) scanAnonBasesExpr(*e.a, out);
    if (e.b) scanAnonBasesExpr(*e.b, out);
    if (e.c) scanAnonBasesExpr(*e.c, out);
    if (e.lbody) scanAnonBasesExpr(*e.lbody, out);
    for (auto& x : e.args)
        if (x) scanAnonBasesExpr(*x, out);
    for (auto& x : e.dims)
        if (x) scanAnonBasesExpr(*x, out);
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock) scanAnonBasesStmt(*s, out);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) scanAnonBasesExpr(*l, out);
        if (arm.guard) scanAnonBasesExpr(*arm.guard, out);
        if (arm.value) scanAnonBasesExpr(*arm.value, out);
        for (auto& s : arm.body) scanAnonBasesStmt(*s, out);
    }
    if (e.anon) {
        for (auto& m : e.anon->members) {
            for (auto& f : m.finits)
                if (f) scanAnonBasesExpr(*f, out);
            if (m.body) scanAnonBasesStmt(*m.body, out);
        }
    }
}

void scanAnonBasesDeclBody(Decl& d, std::set<std::string>& out) {
    for (auto& m : d.members) {
        for (auto& f : m.finits)
            if (f) scanAnonBasesExpr(*f, out);
        if (m.body) scanAnonBasesStmt(*m.body, out);
        if (m.nested) scanAnonBasesDeclBody(*m.nested, out);
    }
    for (auto& c : d.consts) {
        for (auto& a : c.args)
            if (a) scanAnonBasesExpr(*a, out);
        if (c.body) scanAnonBasesDeclBody(*c.body, out);
    }
}

void detectSamDelegates(Ctx& ctx, Unit& u) {
    std::set<std::string> candidates;
    for (auto& d : u.types) detectSamDelegatesDecl(ctx, *d, candidates);
    std::set<std::string> anonBases;
    for (auto& d : u.types) scanAnonBasesDeclBody(*d, anonBases);
    std::vector<Decl*> all;
    for (auto& d : u.types) collectDeclsRecursive(*d, all);
    for (Decl* d : all) {
        std::vector<std::string> refs;
        for (auto& t : d->impls) refs.push_back(firstSeg(t.name));
        if (!d->extends.empty()) refs.push_back(firstSeg(d->extends.name));
        for (auto& r : refs)
            if (candidates.count(r) && r != d->name) candidates.erase(r);
    }
    for (const auto& r : anonBases) candidates.erase(r);
    ctx.samDelegates = candidates;
}

void collectDeclsRecursive(Decl& d, std::vector<Decl*>& out) {
    out.push_back(&d);
    for (auto& m : d.members)
        if (m.mk == 4 && m.nested) collectDeclsRecursive(*m.nested, out);
}

void detectSamDelegatesDecl(Ctx& ctx, Decl& d, std::set<std::string>& candidates) {
    {
        if (d.dk == 1 && mapJavaTypeFqn(d.name, true).empty()) {
            int abstractCount = 0;
            bool ok = true;
            for (auto& m : d.members) {
                if (m.mk == 1) {
                    if (!m.body) abstractCount++;
                    else if (!hasMod(m.mods, "static") && !hasMod(m.mods, "private")) ok = false;
                } else if (m.mk != 4) {
                    ok = false;
                }
            }
            if (abstractCount == 1 && ok) {
                candidates.insert(d.name);
                for (auto& m : d.members)
                    if (m.mk == 1 && !m.body) ctx.samMethodNames[d.name] = m.name;
            }
        }
        for (auto& m : d.members)
            if (m.mk == 4 && m.nested) detectSamDelegatesDecl(ctx, *m.nested, candidates);
    }
}

void classifyTypesDecl(Ctx& ctx, Decl& d) {
    if (d.dk == 2) {
        bool simple = d.impls.empty() && d.members.empty();
        for (auto& c : d.consts)
            if (!c.args.empty() || c.body) simple = false;
        if (simple) ctx.simpleEnums.insert(d.name);
        else ctx.enumClassTypes.insert(d.name);
    }
    if (d.dk == 3) {
        ctx.recordTypes.insert(d.name);
        for (auto& m : d.members)
            if (m.mk == 2 && hasMod(m.mods, "__recordctor") && !m.compactCtor)
                for (auto& p : m.params) ctx.recordComps[d.name].push_back(p.second);
    }
    for (auto& c : d.consts) ctx.enumConstOwner[c.name] = d.name;
    for (auto& m : d.members)
        if (m.mk == 4 && m.nested) classifyTypesDecl(ctx, *m.nested);
}

void classifyTypes(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) classifyTypesDecl(ctx, *d);
}

void countMains(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) {
        for (auto& m : d->members) {
            if (m.mk == 1 && m.name == "main" && hasMod(m.mods, "static")) ctx.mainCount++;
        }
    }
}
TypeRef infoToTypeRef(const TypeInfo& ti) {
    TypeRef t;
    t.name = ti.name.empty() ? "object" : ti.name;
    for (auto& a : ti.args) t.args.push_back(infoToTypeRef(a));
    t.dims = ti.dims;
    return t;
}

struct LocalClassInfo {
    std::string orig;
    std::string gen;
    std::string enclosingCls;
    std::vector<VarInfo> caps;
    bool needThis = false;
};

struct HEnv {
    Ctx& ctx;
    Decl* enclosing;
    std::string enclosingCls;
    std::vector<VarInfo> methodVars;
    std::set<std::string> outerMembers;
    std::map<std::string, LocalClassInfo> localClassMap;
    std::map<std::string, std::string> outerFieldPaths;
    std::map<std::string, std::string> outerMethodPaths;
    std::string currentLocalClassName;
    bool hasEnclosingInstance = false;
    bool hasRet = false;
    TypeRef methodRet;
    int counter = 0;

    HEnv(Ctx& c, Decl* enc, const std::string& cls) : ctx(c), enclosing(enc), enclosingCls(cls) {}
};

void hoistInExpr(Expr& e, HEnv& env);
void hoistInStmt(Stmt& st, HEnv& env);

const std::set<std::string>& staticReceivers() {
    static const std::set<std::string> s = {
        "Math", "System", "Integer", "Long", "Short", "Byte", "Double", "Float",
        "Boolean", "Character", "String", "Objects", "Arrays", "Collections", "Thread",
        "Optional", "UUID", "Instant", "LocalDate", "LocalTime", "LocalDateTime",
        "Duration", "Period", "Pattern", "Matcher", "Files", "Paths", "Locale",
        "StandardCharsets", "Charset", "BigDecimal", "BigInteger", "Stream",
        "Collectors", "StrictMath", "Console", "Enum", "Void", "StringBuilder"};
    return s;
}

void collectLocalsStmt(const Stmt& st, Ctx& ctx, std::vector<VarInfo>& out);

void collectLocalsExpr(const Expr& e, Ctx& ctx, std::vector<VarInfo>& out) {
    if (e.k == EK::Lambda) {
        for (auto& lp : e.lparams) out.push_back({lp.second, toInfo(ctx, lp.first)});
    }
    if (e.obj) collectLocalsExpr(*e.obj, ctx, out);
    if (e.a) collectLocalsExpr(*e.a, ctx, out);
    if (e.b) collectLocalsExpr(*e.b, ctx, out);
    if (e.c) collectLocalsExpr(*e.c, ctx, out);
    if (e.lbody) collectLocalsExpr(*e.lbody, ctx, out);
    for (auto& x : e.args)
        if (x) collectLocalsExpr(*x, ctx, out);
    for (auto& x : e.dims)
        if (x) collectLocalsExpr(*x, ctx, out);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) collectLocalsExpr(*l, ctx, out);
        if (arm.guard) collectLocalsExpr(*arm.guard, ctx, out);
        if (arm.value) collectLocalsExpr(*arm.value, ctx, out);
        for (auto& s : arm.body) collectLocalsStmt(*s, ctx, out);
    }
}

void collectLocalsStmt(const Stmt& st, Ctx& ctx, std::vector<VarInfo>& out) {
    switch (st.k) {
        case SK::LocalVar:
            for (size_t i = 0; i < st.names.size(); i++) {
                TypeRef t = st.ty;
                t.dims += st.dims[i];
                out.push_back({st.names[i], toInfo(ctx, t)});
                if (st.inits[i]) collectLocalsExpr(*st.inits[i], ctx, out);
            }
            break;
        case SK::Block:
            for (auto& s : st.block) collectLocalsStmt(*s, ctx, out);
            break;
        case SK::ExprStmt:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            break;
        case SK::If:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            if (st.body2) collectLocalsStmt(*st.body2, ctx, out);
            break;
        case SK::While:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::DoWhile:
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            break;
        case SK::For:
            for (size_t i = 0; i < st.names.size(); i++) {
                TypeRef t = st.ty;
                t.dims += st.dims[i];
                out.push_back({st.names[i], toInfo(ctx, t)});
            }
            for (auto& x : st.inits)
                if (x) collectLocalsExpr(*x, ctx, out);
            for (auto& x : st.initExprs)
                if (x) collectLocalsExpr(*x, ctx, out);
            for (auto& x : st.updateExprs)
                if (x) collectLocalsExpr(*x, ctx, out);
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::Foreach:
            for (auto& n : st.names) out.push_back({n, toInfo(ctx, st.ty)});
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::Return:
        case SK::Throw:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            break;
        case SK::Try:
            for (size_t i = 0; i < st.resources.size(); i++) {
                out.push_back({st.resources[i].second, toInfo(ctx, st.resources[i].first)});
                if (st.resInits[i]) collectLocalsExpr(*st.resInits[i], ctx, out);
            }
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            for (auto& c : st.catches) {
                std::string vt = c.types.empty() ? "Exception" : c.types[0].text();
                TypeRef vt2;
                vt2.name = vt;
                out.push_back({c.var, toInfo(ctx, vt2)});
                collectLocalsStmt(*c.body, ctx, out);
            }
            if (st.fin) collectLocalsStmt(*st.fin, ctx, out);
            break;
        case SK::Switch:
            if (st.subject) collectLocalsExpr(*st.subject, ctx, out);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) collectLocalsExpr(*l, ctx, out);
                if (arm.guard) collectLocalsExpr(*arm.guard, ctx, out);
                if (arm.value) collectLocalsExpr(*arm.value, ctx, out);
                for (auto& s : arm.body) collectLocalsStmt(*s, ctx, out);
            }
            break;
        case SK::Sync:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::Assert:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.e2) collectLocalsExpr(*st.e2, ctx, out);
            break;
        case SK::Label:
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        default:
            break;
    }
}

void walkIdentsExpr(const Expr& e, const std::set<std::string>& typeNames,
                    std::set<std::string>& declared, std::set<std::string>& used,
                    bool* needThisRef);

void walkIdentsStmt(const Stmt& st, const std::set<std::string>& typeNames,
                    std::set<std::string>& declared, std::set<std::string>& used,
                    bool* needThisRef) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& n : st.names) declared.insert(n);
            for (auto& x : st.inits)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            break;
        case SK::Block:
            for (auto& s : st.block) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
            break;
        case SK::ExprStmt:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            break;
        case SK::If:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            if (st.body2) walkIdentsStmt(*st.body2, typeNames, declared, used, needThisRef);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::For:
            for (auto& n : st.names) declared.insert(n);
            for (auto& x : st.inits)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            for (auto& x : st.initExprs)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            for (auto& x : st.updateExprs)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::Foreach:
            for (auto& n : st.names) declared.insert(n);
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::Return:
        case SK::Throw:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            break;
        case SK::Break:
        case SK::Continue:
        case SK::Empty:
            break;
        case SK::Try:
            for (auto& r : st.resources) declared.insert(r.second);
            for (auto& x : st.resInits)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            for (auto& c : st.catches) {
                declared.insert(c.var);
                walkIdentsStmt(*c.body, typeNames, declared, used, needThisRef);
            }
            if (st.fin) walkIdentsStmt(*st.fin, typeNames, declared, used, needThisRef);
            break;
        case SK::Switch:
            if (st.subject) walkIdentsExpr(*st.subject, typeNames, declared, used, needThisRef);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) walkIdentsExpr(*l, typeNames, declared, used, needThisRef);
                if (arm.guard) walkIdentsExpr(*arm.guard, typeNames, declared, used, needThisRef);
                if (arm.value) walkIdentsExpr(*arm.value, typeNames, declared, used, needThisRef);
                if (!arm.patName.empty()) declared.insert(arm.patName);
                for (auto& s : arm.body) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
            }
            break;
        case SK::Sync:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::Assert:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.e2) walkIdentsExpr(*st.e2, typeNames, declared, used, needThisRef);
            break;
        case SK::Label:
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::LocalClass:
            if (st.lclass) declared.insert(st.lclass->name);
            break;
    }
}

void walkIdentsExpr(const Expr& e, const std::set<std::string>& typeNames,
                    std::set<std::string>& declared, std::set<std::string>& used,
                    bool* needThisRef) {
    if (e.k == EK::Ident) {
        std::string seg = firstSeg(e.name);
        if (seg != "this" && seg != "super" && !typeNames.count(seg) &&
            !staticReceivers().count(seg) && !primitiveTypes().count(seg) &&
            seg != "var" && seg != "record" && seg != "yield" && seg != "sealed")
            used.insert(seg);
        return;
    }
    if (e.k == EK::Call && !e.obj) {
        if (e.name != "this" && e.name != "super") used.insert(firstSeg(e.name));
    }
    if (e.k == EK::Field && e.name == "this" && e.obj) {
        *needThisRef = true;
        return;
    }
    if (e.k == EK::Lambda) {
        for (auto& lp : e.lparams) declared.insert(lp.second);
    }
    if (e.k == EK::InstOf) {
        if (!e.patName.empty()) declared.insert(e.patName);
        for (auto& pc : e.patComps) declared.insert(pc.second);
    }
    if (e.obj) walkIdentsExpr(*e.obj, typeNames, declared, used, needThisRef);
    if (e.a) walkIdentsExpr(*e.a, typeNames, declared, used, needThisRef);
    if (e.b) walkIdentsExpr(*e.b, typeNames, declared, used, needThisRef);
    if (e.c) walkIdentsExpr(*e.c, typeNames, declared, used, needThisRef);
    if (e.lbody) walkIdentsExpr(*e.lbody, typeNames, declared, used, needThisRef);
    for (auto& x : e.args)
        if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
    for (auto& x : e.dims)
        if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) walkIdentsExpr(*l, typeNames, declared, used, needThisRef);
        if (arm.guard) walkIdentsExpr(*arm.guard, typeNames, declared, used, needThisRef);
        if (arm.value) walkIdentsExpr(*arm.value, typeNames, declared, used, needThisRef);
        if (!arm.patName.empty()) declared.insert(arm.patName);
        for (auto& s : arm.body) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
    }
    if (e.anon) {
        for (auto& m : e.anon->members) {
            for (auto& n : m.names) declared.insert(n);
            if (m.mk == 1) declared.insert(m.name);
            for (auto& p : m.params) declared.insert(p.second);
            for (auto& tp : m.tparams) declared.insert(tp.name);
            for (auto& f : m.finits)
                if (f) walkIdentsExpr(*f, typeNames, declared, used, needThisRef);
            if (m.body) walkIdentsStmt(*m.body, typeNames, declared, used, needThisRef);
        }
    }
}

std::unique_ptr<Expr> mkIdent(const std::string& name, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->k = EK::Ident;
    e->name = name;
    e->line = line;
    return e;
}

void makeFieldChain(const std::string& dotted, std::unique_ptr<Expr> base, Expr& out) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        size_t dot = dotted.find('.', start);
        if (dot == std::string::npos) {
            parts.push_back(dotted.substr(start));
            break;
        }
        parts.push_back(dotted.substr(start, dot - start));
        start = dot + 1;
    }
    out.k = EK::Field;
    out.name = parts.back();
    out.obj = std::move(base);
    for (size_t i = parts.size() - 1; i > 0; i--) {
        auto mid = std::make_unique<Expr>();
        mid->k = EK::Field;
        mid->name = parts[i];
        mid->obj = std::move(out.obj);
        out.obj = std::move(mid);
    }
}

std::unique_ptr<Expr> captureBaseExpr(const std::string& path, int line) {
    std::unique_ptr<Expr> base = mkIdent("__this", line);
    size_t start = 0;
    while (start < path.size()) {
        size_t dot = path.find('.', start);
        std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!part.empty()) {
            auto field = std::make_unique<Expr>();
            field->k = EK::Field;
            field->name = part;
            field->obj = std::move(base);
            base = std::move(field);
        }
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return base;
}

void rewriteAnonRefsExpr(Expr& e, const std::set<std::string>& capFields,
                         const std::set<std::string>& capMethods, bool hasThis,
                         const std::map<std::string, std::string>* fieldPaths = nullptr,
                         const std::map<std::string, std::string>* methodPaths = nullptr);

void rewriteAnonRefsStmt(Stmt& st, const std::set<std::string>& capFields,
                         const std::set<std::string>& capMethods, bool hasThis,
                         const std::map<std::string, std::string>* fieldPaths = nullptr,
                         const std::map<std::string, std::string>* methodPaths = nullptr) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& x : st.inits)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Block:
            for (auto& s : st.block) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::ExprStmt:
        case SK::Return:
        case SK::Throw:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::If:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body2) rewriteAnonRefsStmt(*st.body2, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::For:
            for (auto& x : st.inits)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& x : st.initExprs)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& x : st.updateExprs)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Foreach:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Try:
            for (auto& x : st.resInits)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& c : st.catches) rewriteAnonRefsStmt(*c.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.fin) rewriteAnonRefsStmt(*st.fin, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Switch:
            if (st.subject) rewriteAnonRefsExpr(*st.subject, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) rewriteAnonRefsExpr(*l, capFields, capMethods, hasThis, fieldPaths, methodPaths);
                if (arm.guard) rewriteAnonRefsExpr(*arm.guard, capFields, capMethods, hasThis, fieldPaths, methodPaths);
                if (arm.value) rewriteAnonRefsExpr(*arm.value, capFields, capMethods, hasThis, fieldPaths, methodPaths);
                for (auto& s : arm.body) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            }
            break;
        case SK::Sync:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Assert:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.e2) rewriteAnonRefsExpr(*st.e2, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Label:
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        default:
            break;
    }
}

void rewriteAnonRefsExpr(Expr& e, const std::set<std::string>& capFields,
                         const std::set<std::string>& capMethods, bool hasThis,
                         const std::map<std::string, std::string>* fieldPaths,
                         const std::map<std::string, std::string>* methodPaths) {
    if (e.k == EK::Ident && hasThis) {
        std::string seg = firstSeg(e.name);
        if (capFields.count(seg)) {
            if (e.name.size() > seg.size() && e.name[seg.size()] == '.') {
                std::string rest = e.name.substr(seg.size() + 1);
                std::string path;
                if (fieldPaths) {
                    auto it = fieldPaths->find(seg);
                    if (it != fieldPaths->end()) path = it->second;
                }
                auto outer = std::make_unique<Expr>();
                outer->k = EK::Field;
                outer->name = seg;
                outer->obj = captureBaseExpr(path, e.line);
                makeFieldChain(rest, std::move(outer), e);
            } else {
                std::string path;
                if (fieldPaths) {
                    auto it = fieldPaths->find(seg);
                    if (it != fieldPaths->end()) path = it->second;
                }
                e.k = EK::Field;
                e.obj = captureBaseExpr(path, e.line);
            }
            return;
        }
    }
    if (e.k == EK::Call && !e.obj && hasThis && capMethods.count(e.name)) {
        std::string path;
        if (methodPaths) {
            auto it = methodPaths->find(e.name);
            if (it != methodPaths->end()) path = it->second;
        }
        e.obj = captureBaseExpr(path, e.line);
        return;
    }
    if (e.k == EK::Field && e.name == "this" && e.obj && hasThis) {
        e.k = EK::Ident;
        e.name = "__this";
        e.obj.reset();
        return;
    }
    if (e.obj) rewriteAnonRefsExpr(*e.obj, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.a) rewriteAnonRefsExpr(*e.a, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.b) rewriteAnonRefsExpr(*e.b, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.c) rewriteAnonRefsExpr(*e.c, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.lbody) rewriteAnonRefsExpr(*e.lbody, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    for (auto& x : e.args)
        if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    for (auto& x : e.dims)
        if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) rewriteAnonRefsExpr(*l, capFields, capMethods, hasThis, fieldPaths, methodPaths);
        if (arm.guard) rewriteAnonRefsExpr(*arm.guard, capFields, capMethods, hasThis, fieldPaths, methodPaths);
        if (arm.value) rewriteAnonRefsExpr(*arm.value, capFields, capMethods, hasThis, fieldPaths, methodPaths);
        for (auto& s : arm.body) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    }
}

std::set<std::string> chainInstanceFields(Ctx& ctx, const std::string& cls) {
    std::set<std::string> out;
    std::string c = cls;
    int guard = 0;
    while (!c.empty() && guard++ < 32) {
        std::string prefix = c + ".";
        for (auto& f : ctx.instanceFields)
            if (startsWith(f, prefix)) out.insert(f.substr(prefix.size()));
        c = ancestorOf(ctx, c);
    }
    return out;
}

std::set<std::string> chainInstanceMethods(Ctx& ctx, const std::string& cls) {
    std::set<std::string> out;
    std::string c = cls;
    int guard = 0;
    while (!c.empty() && guard++ < 32) {
        std::string prefix = c + ".";
        for (auto& f : ctx.instanceMethods) {
            if (startsWith(f, prefix)) {
                std::string rest = f.substr(prefix.size());
                size_t slash = rest.find('/');
                out.insert(slash == std::string::npos ? rest : rest.substr(0, slash));
            }
        }
        c = ancestorOf(ctx, c);
    }
    return out;
}

struct CapResult {
    std::vector<VarInfo> capLocals;
    std::set<std::string> capFieldNames;
    std::set<std::string> capMethodNames;
    std::map<std::string, std::string> fieldPaths;
    std::map<std::string, std::string> methodPaths;
    bool needThis = false;
};

CapResult computeCaptures(HEnv& env, const std::set<std::string>& declared,
                          const std::set<std::string>& used, bool needThisRef) {
    CapResult r;
    std::map<std::string, VarInfo> vars;
    for (auto& v : env.methodVars) vars[v.name] = v;
    std::set<std::string> instFields = chainInstanceFields(env.ctx, env.enclosingCls);
    std::set<std::string> instMethods = chainInstanceMethods(env.ctx, env.enclosingCls);
    for (const auto& entry : env.outerFieldPaths) instFields.insert(entry.first);
    for (const auto& entry : env.outerMethodPaths) instMethods.insert(entry.first);
    for (auto& n : used) {
        if (declared.count(n)) continue;
        if (vars.count(n)) {
            r.capLocals.push_back(vars[n]);
            continue;
        }
        if (instFields.count(n)) {
            r.capFieldNames.insert(n);
            auto path = env.outerFieldPaths.find(n);
            r.fieldPaths[n] = path == env.outerFieldPaths.end() ? "" : path->second;
        } else if (instMethods.count(n)) {
            r.capMethodNames.insert(n);
            auto path = env.outerMethodPaths.find(n);
            r.methodPaths[n] = path == env.outerMethodPaths.end() ? "" : path->second;
        }
    }
    r.needThis = needThisRef || !r.capFieldNames.empty() || !r.capMethodNames.empty();
    return r;
}

void addCaptureMembers(Decl& g, const CapResult& caps, HEnv& env) {
    for (auto& v : caps.capLocals) {
        Member fm;
        fm.mk = 0;
        fm.mods = {"private", "readonly"};
        fm.ty = infoToTypeRef(v.ty);
        fm.names.push_back(v.name);
        fm.fdims.push_back(0);
        fm.finits.push_back(nullptr);
        g.members.push_back(std::move(fm));
    }
    if (caps.needThis) {
        Member fm;
        fm.mk = 0;
        fm.mods = {"private", "readonly"};
        fm.ty.name = env.enclosingCls;
        fm.names.push_back("__this");
        fm.fdims.push_back(0);
        fm.finits.push_back(nullptr);
        g.members.push_back(std::move(fm));
    }
}

Member buildCaptureCtor(const std::string& clsName, const CapResult& caps,
                        std::vector<std::unique_ptr<Expr>> baseArgs, const std::string& outerCls) {
    Member cm;
    cm.mk = 2;
    cm.name = clsName;
    cm.mods = {"public"};
    for (auto& v : caps.capLocals) cm.params.push_back({infoToTypeRef(v.ty), v.name});
    if (caps.needThis) {
        TypeRef ot;
        ot.name = outerCls.empty() ? "object" : outerCls;
        cm.params.push_back({ot, "__this"});
    }
    cm.baseArgs = std::move(baseArgs);
    auto blk = std::make_unique<Stmt>();
    blk->k = SK::Block;
    for (auto& v : caps.capLocals) {
        auto as = std::make_unique<Stmt>();
        as->k = SK::ExprStmt;
        auto tgt = std::make_unique<Expr>();
        tgt->k = EK::Field;
        tgt->name = v.name;
        tgt->obj = mkIdent("this");
        auto val = mkIdent(v.name);
        auto call = std::make_unique<Expr>();
        call->k = EK::Assign;
        call->op = "=";
        call->a = std::move(tgt);
        call->b = std::move(val);
        as->e = std::move(call);
        blk->block.push_back(std::move(as));
    }
    if (caps.needThis) {
        auto as = std::make_unique<Stmt>();
        as->k = SK::ExprStmt;
        auto tgt = std::make_unique<Expr>();
        tgt->k = EK::Field;
        tgt->name = "__this";
        tgt->obj = mkIdent("this");
        auto call = std::make_unique<Expr>();
        call->k = EK::Assign;
        call->op = "=";
        call->a = std::move(tgt);
        call->b = mkIdent("__this");
        as->e = std::move(call);
        blk->block.push_back(std::move(as));
    }
    cm.body = std::move(blk);
    return cm;
}

void processAnon(Expr& ne, HEnv& env) {
    AnonBody& body = *ne.anon;
    for (auto& m : body.members) {
        for (auto& f : m.finits)
            if (f) hoistInExpr(*f, env);
        if (m.body) hoistInStmt(*m.body, env);
    }
    std::set<std::string> typeNames;
    for (auto& kv : env.ctx.typeByName) typeNames.insert(kv.first);
    std::set<std::string> declared, used;
    bool needThisRef = false;
    for (auto& m : body.members) {
        for (auto& n : m.names) declared.insert(n);
        if (m.mk == 1) declared.insert(m.name);
        for (auto& p : m.params) declared.insert(p.second);
        for (auto& tp : m.tparams) declared.insert(tp.name);
        for (auto& f : m.finits)
            if (f) walkIdentsExpr(*f, typeNames, declared, used, &needThisRef);
        if (m.body) walkIdentsStmt(*m.body, typeNames, declared, used, &needThisRef);
    }
    CapResult caps = computeCaptures(env, declared, used, needThisRef);
    caps.needThis = caps.needThis || env.hasEnclosingInstance;
    for (auto& m : body.members) {
        for (auto& f : m.finits)
            if (f) rewriteAnonRefsExpr(*f, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                   &caps.fieldPaths, &caps.methodPaths);
        if (m.body) rewriteAnonRefsStmt(*m.body, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                        &caps.fieldPaths, &caps.methodPaths);
    }
    bool iface = isIfaceType(env.ctx, ne.ty);
    std::string genName = "__Anon" + std::to_string(env.counter++);
    auto g = std::make_unique<Decl>();
    g->dk = 0;
    g->mods = {"private", "sealed"};
    g->name = genName;
    TypeRef base = ne.ty;
    if (iface) g->impls.push_back(base);
    else if (!base.name.empty() && base.name != "object") g->extends = base;
    addCaptureMembers(*g, caps, env);
    g->members.push_back(buildCaptureCtor(genName, caps, std::move(ne.args), env.enclosingCls));
    std::set<std::string> capNames;
    for (auto& v : caps.capLocals) capNames.insert(v.name);
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> movedInits;
    for (auto& m : body.members) {
        if (m.mk != 0) continue;
        for (size_t fi = 0; fi < m.finits.size(); fi++) {
            if (m.finits[fi] && refsCaptureOrThis(*m.finits[fi], capNames))
                movedInits.push_back({m.names[fi], std::move(m.finits[fi])});
        }
    }
    for (auto& m : body.members) g->members.push_back(std::move(m));
    if (!movedInits.empty()) {
        for (auto& gm : g->members) {
            if (gm.mk != 2) continue;
            appendFieldInitsToCtor(gm, movedInits, capNames);
        }
    }
    Member nm;
    nm.mk = 4;
    nm.nested = std::move(g);
    nm.line = ne.line;
    env.enclosing->members.push_back(std::move(nm));
    ne.ty = TypeRef();
    ne.ty.name = genName;
    ne.anon.reset();
    for (auto& v : caps.capLocals) ne.args.push_back(mkIdent(v.name, ne.line));
    if (caps.needThis) ne.args.push_back(mkIdent("this", ne.line));
}

TypeRef resolveSemType(HEnv& env, const Expr& e);

bool samForKeptIface(Ctx& ctx, const std::string& simple, std::string& mName, TypeRef& mRet,
                     std::vector<std::pair<TypeRef, std::string>>& mParams) {
    auto it = ctx.typeByName.find(simple);
    if (it == ctx.typeByName.end()) return false;
    Decl* d = it->second;
    if (d->dk != 1) return false;
    if (ctx.samDelegates.count(simple)) return false;
    for (auto& m : d->members) {
        if (m.mk == 1 && !m.body && !hasMod(m.mods, "static")) {
            mName = m.name;
            mRet = m.ty;
            mParams = m.params;
            return true;
        }
    }
    return false;
}

void wrapSamTarget(std::unique_ptr<Expr>& slot, HEnv& env, const TypeRef& target) {
    if (!slot || target.name.empty() || target.dims != 0 || target.isVar || target.wildcard) return;
    if (slot->k != EK::Lambda && slot->k != EK::MRef) return;
    std::string mName;
    TypeRef mRet;
    std::vector<std::pair<TypeRef, std::string>> mParams;
    if (!samForKeptIface(env.ctx, firstSeg(target.name), mName, mRet, mParams)) return;
    auto ne = std::make_unique<Expr>();
    ne->k = EK::New;
    ne->line = slot->line;
    ne->ty = target;
    ne->ty.dims = 0;
    auto anon = std::make_unique<AnonBody>();
    anon->base = ne->ty;
    anon->line = slot->line;
    Member m;
    m.mk = 1;
    m.name = mName;
    m.mods.push_back("public");
    m.ty = mRet;
    m.line = slot->line;
    if (slot->k == EK::Lambda) {
        Expr& lam = *slot;
        size_t n = std::min(lam.lparams.size(), mParams.size());
        for (size_t i = 0; i < lam.lparams.size(); i++) {
            TypeRef pt = i < n ? mParams[i].first : TypeRef();
            if (pt.isVar) pt = TypeRef();
            m.params.push_back({pt, lam.lparams[i].second});
        }
        auto blk = std::make_unique<Stmt>();
        blk->k = SK::Block;
        blk->line = slot->line;
        if (lam.lbodyIsBlock) {
            blk->block = std::move(lam.lblock);
        } else if (lam.lbody) {
            auto ret = std::make_unique<Stmt>();
            ret->k = SK::Return;
            ret->line = lam.lbody->line;
            ret->e = std::move(lam.lbody);
            blk->block.push_back(std::move(ret));
        }
        m.body = std::move(blk);
    } else {
        m.params = mParams;
        Expr& mr = *slot;
        auto call = std::make_unique<Expr>();
        call->k = EK::Call;
        call->name = mr.name;
        call->line = mr.line;
        if (mr.obj) {
            std::string objName = mr.obj->k == EK::Ident ? mr.obj->name : "";
            if (mr.obj->k == EK::This || objName == "this") {
                auto th = std::make_unique<Expr>();
                th->k = EK::This;
                call->obj = std::move(th);
            } else if (!objName.empty() &&
                       (env.ctx.typeByName.count(firstSeg(objName)) || staticReceivers().count(firstSeg(objName)))) {
                auto st = std::make_unique<Expr>();
                st->k = EK::Ident;
                st->name = objName;
                call->obj = std::move(st);
            } else if (!mParams.empty()) {
                call->obj = mkIdent(mParams[0].second, mr.line);
            } else {
                call->obj = cloneExpr(*mr.obj);
            }
        }
        for (auto& p : mParams) call->args.push_back(mkIdent(p.second, mr.line));
        auto ret = std::make_unique<Stmt>();
        ret->k = SK::Return;
        ret->line = mr.line;
        ret->e = std::move(call);
        auto blk = std::make_unique<Stmt>();
        blk->k = SK::Block;
        blk->line = mr.line;
        blk->block.push_back(std::move(ret));
        m.body = std::move(blk);
    }
    anon->members.push_back(std::move(m));
    ne->anon = std::move(anon);
    processAnon(*ne, env);
    *slot = std::move(*ne);
}

TypeRef resolveSemType(HEnv& env, const Expr& e) {
    switch (e.k) {
        case EK::This: {
            TypeRef t;
            t.name = env.enclosingCls;
            return t;
        }
        case EK::Ident: {
            std::string base = firstSeg(e.name);
            if (base == "this" || base == "super") {
                TypeRef t;
                t.name = base == "this" ? env.enclosingCls : ancestorOf(env.ctx, env.enclosingCls);
                return t;
            }
            for (size_t i = env.methodVars.size(); i > 0; i--)
                if (env.methodVars[i - 1].name == base) return infoToTypeRef(env.methodVars[i - 1].ty);
            auto ft = env.ctx.classFieldType.find(env.enclosingCls + "." + base);
            if (ft != env.ctx.classFieldType.end()) return ft->second;
            std::string anc = ancestorOf(env.ctx, env.enclosingCls);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                auto ft2 = env.ctx.classFieldType.find(anc + "." + base);
                if (ft2 != env.ctx.classFieldType.end()) return ft2->second;
                anc = ancestorOf(env.ctx, anc);
            }
            if (env.ctx.enumConstOwner.count(base)) {
                TypeRef t;
                t.name = env.ctx.enumConstOwner[base];
                return t;
            }
            return TypeRef();
        }
        case EK::Field: {
            if (!e.obj) return TypeRef();
            TypeRef ot = resolveSemType(env, *e.obj);
            if (ot.name.empty()) return TypeRef();
            std::string base = firstSeg(ot.name);
            auto ft = env.ctx.classFieldType.find(base + "." + e.name);
            if (ft != env.ctx.classFieldType.end()) return ft->second;
            std::string anc = ancestorOf(env.ctx, base);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                auto ft2 = env.ctx.classFieldType.find(anc + "." + e.name);
                if (ft2 != env.ctx.classFieldType.end()) return ft2->second;
                anc = ancestorOf(env.ctx, anc);
            }
            return TypeRef();
        }
        case EK::Call: {
            std::string cls;
            if (e.obj) {
                TypeRef ot = resolveSemType(env, *e.obj);
                if (ot.name.empty()) return TypeRef();
                cls = firstSeg(ot.name);
            } else {
                cls = env.enclosingCls;
            }
            auto mit = env.ctx.methodReturns.find(cls);
            if (mit != env.ctx.methodReturns.end()) {
                auto it2 = mit->second.find(e.name + "/" + std::to_string(e.args.size()));
                if (it2 != mit->second.end()) return it2->second;
            }
            return TypeRef();
        }
        case EK::Cast:
            return e.ty;
        case EK::New:
            return e.ty;
        default:
            return TypeRef();
    }
}

std::vector<TypeRef> resolveParamTypes(HEnv& env, const Expr& call) {
    std::string cls;
    if (call.obj) {
        TypeRef ot = resolveSemType(env, *call.obj);
        if (ot.name.empty()) return {};
        cls = firstSeg(ot.name);
    } else {
        cls = env.enclosingCls;
    }
    auto it = env.ctx.methodParamTypes.find(cls + "." + call.name + "/" + std::to_string(call.args.size()));
    if (it != env.ctx.methodParamTypes.end()) return it->second;
    return {};
}

void hoistInExpr(Expr& e, HEnv& env);
std::vector<VarInfo> buildMethodVars(Ctx& ctx, Decl& cls, Member& m);

void remapLocalType(TypeRef& type, HEnv& env) {
    auto it = env.localClassMap.find(type.name);
    if (it != env.localClassMap.end()) type.name = it->second.gen;
    for (auto& arg : type.args) remapLocalType(arg, env);
}

void hoistLocalTypesInDecl(Decl& d, HEnv& env) {
    remapLocalType(d.extends, env);
    for (auto& type : d.impls) remapLocalType(type, env);
    for (auto& tp : d.tparams)
        if (tp.hasBound) remapLocalType(tp.bound, env);
    for (auto& ec : d.consts) {
        for (auto& arg : ec.args)
            if (arg) hoistInExpr(*arg, env);
        if (ec.body) hoistLocalTypesInDecl(*ec.body, env);
    }
    for (auto& m : d.members) {
        remapLocalType(m.ty, env);
        for (auto& param : m.params) remapLocalType(param.first, env);
        for (auto& type : m.throws) remapLocalType(type, env);
        for (auto& arg : m.baseArgs)
            if (arg) hoistInExpr(*arg, env);
        for (auto& init : m.finits)
            if (init) hoistInExpr(*init, env);
        if (m.body) hoistInStmt(*m.body, env);
        if (m.nested) {
            HEnv nestedEnv = env;
            for (const auto& kv : env.localClassMap) {
                if (kv.second.gen == m.nested->name) {
                    nestedEnv.currentLocalClassName = kv.first;
                    break;
                }
            }
            hoistLocalTypesInDecl(*m.nested, nestedEnv);
        }
    }
}

bool refsCaptureOrThis(const Expr& e, const std::set<std::string>& names);
bool refsCaptureOrThisStmt(const Stmt& st, const std::set<std::string>& names);

bool refsCaptureOrThis(const Expr& e, const std::set<std::string>& names) {
    if (e.k == EK::Ident) {
        std::string seg = firstSeg(e.name);
        return seg == "this" || seg == "super" || names.count(seg) > 0;
    }
    if (e.k == EK::This) return true;
    if (e.obj && refsCaptureOrThis(*e.obj, names)) return true;
    if (e.a && refsCaptureOrThis(*e.a, names)) return true;
    if (e.b && refsCaptureOrThis(*e.b, names)) return true;
    if (e.c && refsCaptureOrThis(*e.c, names)) return true;
    if (e.lbody && refsCaptureOrThis(*e.lbody, names)) return true;
    for (auto& x : e.args)
        if (x && refsCaptureOrThis(*x, names)) return true;
    for (auto& x : e.dims)
        if (x && refsCaptureOrThis(*x, names)) return true;
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock)
            if (refsCaptureOrThisStmt(*s, names)) return true;
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l && refsCaptureOrThis(*l, names)) return true;
        if (arm.guard && refsCaptureOrThis(*arm.guard, names)) return true;
        if (arm.value && refsCaptureOrThis(*arm.value, names)) return true;
        for (auto& s : arm.body)
            if (refsCaptureOrThisStmt(*s, names)) return true;
    }
    return false;
}

bool refsCaptureOrThisStmt(const Stmt& st, const std::set<std::string>& names) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& x : st.inits)
                if (x && refsCaptureOrThis(*x, names)) return true;
            return false;
        case SK::Block:
            for (auto& s : st.block)
                if (refsCaptureOrThisStmt(*s, names)) return true;
            return false;
        case SK::ExprStmt:
        case SK::Return:
        case SK::Throw:
            return st.e && refsCaptureOrThis(*st.e, names);
        case SK::If:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            if (st.body && refsCaptureOrThisStmt(*st.body, names)) return true;
            if (st.body2 && refsCaptureOrThisStmt(*st.body2, names)) return true;
            return false;
        case SK::While:
        case SK::DoWhile:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::For:
            for (auto& x : st.inits)
                if (x && refsCaptureOrThis(*x, names)) return true;
            for (auto& x : st.initExprs)
                if (x && refsCaptureOrThis(*x, names)) return true;
            for (auto& x : st.updateExprs)
                if (x && refsCaptureOrThis(*x, names)) return true;
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::Foreach:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::Try:
            for (auto& x : st.resInits)
                if (x && refsCaptureOrThis(*x, names)) return true;
            if (st.body && refsCaptureOrThisStmt(*st.body, names)) return true;
            for (auto& c : st.catches)
                if (refsCaptureOrThisStmt(*c.body, names)) return true;
            return st.fin && refsCaptureOrThisStmt(*st.fin, names);
        case SK::Switch:
            if (st.subject && refsCaptureOrThis(*st.subject, names)) return true;
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l && refsCaptureOrThis(*l, names)) return true;
                if (arm.guard && refsCaptureOrThis(*arm.guard, names)) return true;
                if (arm.value && refsCaptureOrThis(*arm.value, names)) return true;
                for (auto& s : arm.body)
                    if (refsCaptureOrThisStmt(*s, names)) return true;
            }
            return false;
        case SK::Sync:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::Assert:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.e2 && refsCaptureOrThis(*st.e2, names);
        case SK::Label:
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        default:
            return false;
    }
}

std::unique_ptr<Stmt> makeFieldAssign(const std::string& name, std::unique_ptr<Expr> value) {
    auto as = std::make_unique<Stmt>();
    as->k = SK::ExprStmt;
    auto tgt = std::make_unique<Expr>();
    tgt->k = EK::Field;
    tgt->name = name;
    tgt->obj = mkIdent("this");
    auto assign = std::make_unique<Expr>();
    assign->k = EK::Assign;
    assign->op = "=";
    assign->a = std::move(tgt);
    assign->b = std::move(value);
    as->e = std::move(assign);
    return as;
}

void appendFieldInitsToCtor(Member& ctor, const std::vector<std::pair<std::string, std::unique_ptr<Expr>>>& inits,
                            const std::set<std::string>& capNames) {
    if (!ctor.body || ctor.body->k != SK::Block) return;
    size_t pos = 0;
    if (!ctor.body->block.empty()) {
        const Stmt& first = *ctor.body->block.front();
        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) pos = 1;
    }
    while (pos < ctor.body->block.size()) {
        const Stmt& s = *ctor.body->block[pos];
        if (s.k == SK::ExprStmt && s.e && s.e->k == EK::Assign) {
            const Expr& tgt = *s.e->a;
            const Expr& val = *s.e->b;
            if (tgt.k == EK::Field && tgt.obj && tgt.obj->k == EK::Ident && tgt.obj->name == "this" &&
                val.k == EK::Ident && val.name == tgt.name &&
                (tgt.name == "__this" || capNames.count(tgt.name) > 0)) {
                pos++;
                continue;
            }
        }
        break;
    }
    for (auto it = inits.rbegin(); it != inits.rend(); ++it) {
        ctor.body->block.insert(ctor.body->block.begin() + static_cast<std::ptrdiff_t>(pos),
                                makeFieldAssign(it->first, cloneExpr(*it->second)));
    }
}

void processLocalClass(Stmt& st, HEnv& env) {
    if (!st.lclass) {
        st.k = SK::Empty;
        return;
    }
    Decl& d = *st.lclass;
    std::string origName = d.name;
    std::string genName = "__LC" + std::to_string(env.counter++) + "_" + origName;
    HEnv bodyEnv = env;
    bodyEnv.enclosing = &d;
    bodyEnv.enclosingCls = genName;
    bodyEnv.currentLocalClassName = origName;
    bodyEnv.hasEnclosingInstance = true;
    bodyEnv.outerFieldPaths.clear();
    bodyEnv.outerMethodPaths.clear();
    if (env.hasEnclosingInstance) {
        for (const auto& entry : env.outerFieldPaths)
            bodyEnv.outerFieldPaths[entry.first] = "__this" + (entry.second.empty() ? "" : "." + entry.second);
        for (const auto& entry : env.outerMethodPaths)
            bodyEnv.outerMethodPaths[entry.first] = "__this" + (entry.second.empty() ? "" : "." + entry.second);
    }
    for (const auto& member : d.members) {
        if (member.mk == 0 && !hasMod(member.mods, "static")) {
            for (const auto& name : member.names) bodyEnv.outerFieldPaths[name] = "";
        } else if (member.mk == 1 && !hasMod(member.mods, "static")) {
            bodyEnv.outerMethodPaths[member.name] = "";
        }
    }
    const size_t originalMemberCount = d.members.size();
    for (size_t index = 0; index < originalMemberCount; ++index) {
        int memberKind = d.members[index].mk;
        bool isStatic = hasMod(d.members[index].mods, "static") || d.members[index].isStaticInit;
        Stmt* body = d.members[index].body.get();
        std::vector<Expr*> initializers;
        for (auto& initializer : d.members[index].finits)
            if (initializer) initializers.push_back(initializer.get());
        HEnv memberEnv = bodyEnv;
        if (memberKind == 1 || memberKind == 2) {
            memberEnv.hasEnclosingInstance = !isStatic;
            if (memberKind == 1 && !d.members[index].ty.empty() && !d.members[index].ty.isVar) {
                memberEnv.hasRet = true;
                memberEnv.methodRet = d.members[index].ty;
            }
            std::vector<VarInfo> vars = buildMethodVars(env.ctx, d, d.members[index]);
            memberEnv.methodVars.insert(memberEnv.methodVars.end(), vars.begin(), vars.end());
        } else if (memberKind == 0 || memberKind == 3) {
            memberEnv.hasEnclosingInstance = !isStatic;
        }
        for (Expr* initializer : initializers) {
            if (initializer) hoistInExpr(*initializer, memberEnv);
        }
        if (body) hoistInStmt(*body, memberEnv);
        bodyEnv.counter = std::max(bodyEnv.counter, memberEnv.counter);
        for (auto& entry : memberEnv.localClassMap) bodyEnv.localClassMap[entry.first] = entry.second;
    }
    env.counter = std::max(env.counter, bodyEnv.counter);
    std::set<std::string> typeNames;
    for (auto& kv : env.ctx.typeByName) typeNames.insert(kv.first);
    std::set<std::string> declared, used;
    bool needThisRef = false;
    for (size_t index = 0; index < originalMemberCount; ++index) {
        const Member& member = d.members[index];
        for (auto& name : member.names) declared.insert(name);
        if (member.mk == 1 || member.mk == 2) {
            if (member.mk == 1) declared.insert(member.name);
            for (auto& param : member.params) declared.insert(param.second);
            for (auto& tp : member.tparams) declared.insert(tp.name);
            if (member.body) {
                std::vector<VarInfo> locals;
                collectLocalsStmt(*member.body, env.ctx, locals);
                for (auto& local : locals) declared.insert(local.name);
            }
        }
        for (auto& initializer : member.finits)
            if (initializer) walkIdentsExpr(*initializer, typeNames, declared, used, &needThisRef);
        if (member.body) walkIdentsStmt(*member.body, typeNames, declared, used, &needThisRef);
    }
    CapResult caps = computeCaptures(env, declared, used, needThisRef);
    caps.needThis = caps.needThis || env.hasEnclosingInstance;
    const LocalClassInfo* localBase = nullptr;
    auto baseIt = env.localClassMap.find(d.extends.name);
    if (baseIt != env.localClassMap.end()) {
        localBase = &baseIt->second;
        for (const auto& captured : localBase->caps) {
            auto found = std::find_if(caps.capLocals.begin(), caps.capLocals.end(),
                                      [&](const VarInfo& value) { return value.name == captured.name; });
            if (found == caps.capLocals.end()) caps.capLocals.push_back(captured);
        }
        caps.needThis = caps.needThis || localBase->needThis;
    }
    std::vector<std::unique_ptr<Expr>> localBaseArgs;
    if (localBase) {
        for (const auto& captured : localBase->caps)
            localBaseArgs.push_back(mkIdent(captured.name, st.line));
        if (localBase->needThis) localBaseArgs.push_back(mkIdent("__this", st.line));
    }
    for (size_t index = 0; index < originalMemberCount; ++index) {
        Member& member = d.members[index];
        for (auto& initializer : member.finits) {
            if (initializer)
                rewriteAnonRefsExpr(*initializer, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                    &caps.fieldPaths, &caps.methodPaths);
        }
        if (member.body)
            rewriteAnonRefsStmt(*member.body, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                &caps.fieldPaths, &caps.methodPaths);
    }
    bool hasCtor = false;
    for (size_t index = 0; index < originalMemberCount; ++index)
        if (d.members[index].mk == 2) hasCtor = true;
    if (!caps.capLocals.empty() || caps.needThis || !hasCtor) {
        for (size_t index = 0; index < originalMemberCount; ++index) {
            Member& member = d.members[index];
            if (member.mk != 2) continue;
            for (auto& variable : caps.capLocals)
                member.params.push_back({infoToTypeRef(variable.ty), variable.name});
            if (caps.needThis) {
                TypeRef outerType;
                outerType.name = env.enclosingCls;
                member.params.push_back({outerType, "__this"});
            }
            bool hasExplicitSuperCall = false;
            if (member.body && member.body->k == SK::Block && !member.body->block.empty()) {
                Stmt& first = *member.body->block.front();
                if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) {
                    Expr& ctorCall = *first.e;
                    if (ctorCall.name == "super") {
                        hasExplicitSuperCall = true;
                        for (const auto& baseArg : localBaseArgs) {
                            auto copy = std::make_unique<Expr>();
                            copy->k = baseArg->k;
                            copy->name = baseArg->name;
                            copy->line = baseArg->line;
                            ctorCall.args.push_back(std::move(copy));
                        }
                    } else if (ctorCall.name == "this") {
                        for (const auto& captured : caps.capLocals)
                            ctorCall.args.push_back(mkIdent(captured.name, st.line));
                        if (caps.needThis) ctorCall.args.push_back(mkIdent("__this", st.line));
                    }
                }
            }
            if (!hasExplicitSuperCall) {
                for (const auto& baseArg : localBaseArgs) {
                    auto copy = std::make_unique<Expr>();
                    copy->k = baseArg->k;
                    copy->name = baseArg->name;
                    copy->line = baseArg->line;
                    member.baseArgs.push_back(std::move(copy));
                }
            }
            if (member.body && member.body->k == SK::Block) {
                for (auto variable = caps.capLocals.rbegin(); variable != caps.capLocals.rend(); ++variable) {
                    auto assignment = std::make_unique<Stmt>();
                    assignment->k = SK::ExprStmt;
                    auto target = std::make_unique<Expr>();
                    target->k = EK::Field;
                    target->name = variable->name;
                    target->obj = mkIdent("this");
                    auto assignExpr = std::make_unique<Expr>();
                    assignExpr->k = EK::Assign;
                    assignExpr->op = "=";
                    assignExpr->a = std::move(target);
                    assignExpr->b = mkIdent(variable->name);
                    assignment->e = std::move(assignExpr);
                    size_t insertion = 0;
                    if (!member.body->block.empty()) {
                        const Stmt& first = *member.body->block.front();
                        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) insertion = 1;
                    }
                    member.body->block.insert(member.body->block.begin() + static_cast<std::ptrdiff_t>(insertion),
                                              std::move(assignment));
                }
                if (caps.needThis) {
                    auto assignment = std::make_unique<Stmt>();
                    assignment->k = SK::ExprStmt;
                    auto target = std::make_unique<Expr>();
                    target->k = EK::Field;
                    target->name = "__this";
                    target->obj = mkIdent("this");
                    auto assignExpr = std::make_unique<Expr>();
                    assignExpr->k = EK::Assign;
                    assignExpr->op = "=";
                    assignExpr->a = std::move(target);
                    assignExpr->b = mkIdent("__this");
                    assignment->e = std::move(assignExpr);
                    size_t insertion = 0;
                    if (!member.body->block.empty()) {
                        const Stmt& first = *member.body->block.front();
                        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) insertion = 1;
                    }
                    member.body->block.insert(member.body->block.begin() + static_cast<std::ptrdiff_t>(insertion),
                                              std::move(assignment));
                }
            }
        }
        if (!hasCtor) {
            Member constructor = buildCaptureCtor(d.name, caps, std::move(localBaseArgs), env.enclosingCls);
            d.members.insert(d.members.begin(), std::move(constructor));
        }
        if (caps.needThis) {
            Member outerField;
            outerField.mk = 0;
            outerField.mods = {"private", "readonly"};
            outerField.ty.name = env.enclosingCls;
            outerField.names.push_back("__this");
            outerField.fdims.push_back(0);
            outerField.finits.push_back(nullptr);
            d.members.insert(d.members.begin(), std::move(outerField));
        }
        for (auto variable = caps.capLocals.rbegin(); variable != caps.capLocals.rend(); ++variable) {
            Member capturedField;
            capturedField.mk = 0;
            capturedField.mods = {"private", "readonly"};
            capturedField.ty = infoToTypeRef(variable->ty);
            capturedField.names.push_back(variable->name);
            capturedField.fdims.push_back(0);
            capturedField.finits.push_back(nullptr);
            d.members.insert(d.members.begin(), std::move(capturedField));
        }
    }
    std::set<std::string> capNames;
    for (auto& v : caps.capLocals) capNames.insert(v.name);
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> movedInits;
    for (auto& member : d.members) {
        if (member.mk != 0) continue;
        for (size_t fi = 0; fi < member.finits.size(); fi++) {
            if (member.finits[fi] && refsCaptureOrThis(*member.finits[fi], capNames))
                movedInits.push_back({member.names[fi], std::move(member.finits[fi])});
        }
    }
    if (!movedInits.empty()) {
        for (auto& member : d.members) {
            if (member.mk != 2) continue;
            appendFieldInitsToCtor(member, movedInits, capNames);
        }
    }
    LocalClassInfo lci;
    lci.orig = origName;
    lci.gen = genName;
    lci.enclosingCls = env.enclosingCls;
    lci.caps = caps.capLocals;
    lci.needThis = caps.needThis;
    env.localClassMap[origName] = lci;
    bodyEnv.localClassMap[origName] = lci;
    bodyEnv.currentLocalClassName = origName;
    hoistLocalTypesInDecl(d, bodyEnv);
    d.mods.insert(d.mods.begin(), "private");
    d.mods.push_back("sealed");
    d.name = genName;
    Member nestedMember;
    nestedMember.mk = 4;
    nestedMember.nested = std::move(st.lclass);
    nestedMember.line = st.line;
    env.enclosing->members.push_back(std::move(nestedMember));
    st.k = SK::Empty;
}

void hoistInExpr(Expr& e, HEnv& env) {
    if (e.k == EK::Cast) wrapSamTarget(e.a, env, e.ty);
    if (e.k == EK::Call) {
        std::vector<TypeRef> ptypes = resolveParamTypes(env, e);
        for (size_t i = 0; i < ptypes.size() && i < e.args.size(); i++)
            wrapSamTarget(e.args[i], env, ptypes[i]);
    }
    if (e.k == EK::New && !e.anon) {
        auto pit = env.ctx.methodParamTypes.find(firstSeg(e.ty.name) + ".<init>/" +
                                                 std::to_string(e.args.size()));
        for (size_t i = 0; pit != env.ctx.methodParamTypes.end() && i < pit->second.size() &&
                           i < e.args.size();
             i++)
            wrapSamTarget(e.args[i], env, pit->second[i]);
    }
    if (e.k == EK::New && !e.anon) {
        auto it = env.localClassMap.find(e.ty.name);
        if (it != env.localClassMap.end()) {
            e.ty.name = it->second.gen;
            for (auto& v : it->second.caps) e.args.push_back(mkIdent(v.name, e.line));
            if (it->second.needThis) {
                std::unique_ptr<Expr> outerThis = mkIdent("this", e.line);
                std::string current = env.currentLocalClassName;
                std::set<std::string> visited;
                while (!current.empty() && visited.insert(current).second) {
                    auto currentInfo = env.localClassMap.find(current);
                    if (currentInfo == env.localClassMap.end()) break;
                    if (currentInfo->second.gen == it->second.enclosingCls) break;
                    if (!currentInfo->second.needThis) break;
                    auto field = std::make_unique<Expr>();
                    field->k = EK::Field;
                    field->name = "__this";
                    field->obj = std::move(outerThis);
                    outerThis = std::move(field);
                    if (currentInfo->second.enclosingCls == it->second.enclosingCls) break;
                    auto parentInfo = std::find_if(
                        env.localClassMap.begin(), env.localClassMap.end(),
                        [&](const auto& entry) { return entry.second.gen == currentInfo->second.enclosingCls; });
                    if (parentInfo == env.localClassMap.end()) break;
                    current = parentInfo->first;
                }
                e.args.push_back(std::move(outerThis));
            }
        } else {
            remapLocalType(e.ty, env);
        }
    } else {
        remapLocalType(e.ty, env);
    }
    for (auto& type : e.targs) remapLocalType(type, env);
    for (auto& param : e.lparams) remapLocalType(param.first, env);
    for (auto& type : e.patComps) remapLocalType(type.first, env);
    if (e.anon) remapLocalType(e.anon->base, env);
    if (e.obj) hoistInExpr(*e.obj, env);
    if (e.a) hoistInExpr(*e.a, env);
    if (e.b) hoistInExpr(*e.b, env);
    if (e.c) hoistInExpr(*e.c, env);
    if (e.lbody) hoistInExpr(*e.lbody, env);
    for (auto& x : e.args)
        if (x) hoistInExpr(*x, env);
    for (auto& x : e.dims)
        if (x) hoistInExpr(*x, env);
    if (e.lbodyIsBlock) {
        bool savedHas = env.hasRet;
        TypeRef savedRet = env.methodRet;
        env.hasRet = false;
        env.methodRet = TypeRef();
        for (auto& s : e.lblock) hoistInStmt(*s, env);
        env.hasRet = savedHas;
        env.methodRet = savedRet;
    }
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) hoistInExpr(*l, env);
        if (arm.guard) hoistInExpr(*arm.guard, env);
        if (arm.value) hoistInExpr(*arm.value, env);
        for (auto& s : arm.body) hoistInStmt(*s, env);
    }
    if (e.k == EK::New && e.anon) processAnon(e, env);
}

void hoistInStmt(Stmt& st, HEnv& env) {
    remapLocalType(st.ty, env);
    for (auto& resource : st.resources) remapLocalType(resource.first, env);
    for (auto& c : st.catches)
        for (auto& type : c.types) remapLocalType(type, env);
    switch (st.k) {
        case SK::LocalClass:
            processLocalClass(st, env);
            return;
        case SK::Block:
            for (auto& s : st.block) hoistInStmt(*s, env);
            break;
        case SK::ExprStmt:
            if (st.e && st.e->k == EK::Assign && st.e->op == "=") {
                TypeRef lhsTy = resolveSemType(env, *st.e->a);
                wrapSamTarget(st.e->b, env, lhsTy);
            }
            if (st.e) hoistInExpr(*st.e, env);
            break;
        case SK::LocalVar:
            if (!st.ty.isVar)
                for (auto& x : st.inits)
                    if (x) wrapSamTarget(x, env, st.ty);
            for (auto& x : st.inits)
                if (x) hoistInExpr(*x, env);
            break;
        case SK::If:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            if (st.body2) hoistInStmt(*st.body2, env);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::For:
            if (!st.ty.isVar)
                for (auto& x : st.inits)
                    if (x) wrapSamTarget(x, env, st.ty);
            for (auto& x : st.initExprs)
                if (x) {
                    if (x->k == EK::Assign && x->op == "=") {
                        TypeRef lhsTy = resolveSemType(env, *x->a);
                        wrapSamTarget(x->b, env, lhsTy);
                    }
                    hoistInExpr(*x, env);
                }
            for (auto& x : st.updateExprs)
                if (x) hoistInExpr(*x, env);
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::Foreach:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::Return:
        case SK::Throw:
            if (st.e && st.k == SK::Return && env.hasRet) wrapSamTarget(st.e, env, env.methodRet);
            if (st.e) hoistInExpr(*st.e, env);
            break;
        case SK::Try:
            for (auto& x : st.resInits)
                if (x) hoistInExpr(*x, env);
            if (st.body) hoistInStmt(*st.body, env);
            for (auto& c : st.catches) hoistInStmt(*c.body, env);
            if (st.fin) hoistInStmt(*st.fin, env);
            break;
        case SK::Switch:
            if (st.subject) hoistInExpr(*st.subject, env);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) hoistInExpr(*l, env);
                if (arm.guard) hoistInExpr(*arm.guard, env);
                if (arm.value) hoistInExpr(*arm.value, env);
                for (auto& s : arm.body) hoistInStmt(*s, env);
            }
            break;
        case SK::Sync:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::Assert:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.e2) hoistInExpr(*st.e2, env);
            break;
        case SK::Label:
            if (st.body) hoistInStmt(*st.body, env);
            break;
        default:
            break;
    }
}

std::vector<VarInfo> buildMethodVars(Ctx& ctx, Decl& cls, Member& m) {
    (void)cls;
    std::vector<VarInfo> out;
    for (auto& p : m.params) out.push_back({p.second, toInfo(ctx, p.first)});
    if (m.body) collectLocalsStmt(*m.body, ctx, out);
    return out;
}

std::set<std::string> outerMemberNames(Ctx& ctx, const std::string& cls) {
    std::set<std::string> out = chainInstanceFields(ctx, cls);
    std::set<std::string> ms = chainInstanceMethods(ctx, cls);
    out.insert(ms.begin(), ms.end());
    return out;
}

void hoistInDecl(Ctx& ctx, Decl& cls) {
    size_t processed = 0;
    while (processed < cls.members.size()) {
        Member& m = cls.members[processed];
        int mk = m.mk;
        Stmt* body = m.body.get();
        Decl* nested = m.nested.get();
        std::vector<Expr*> finits;
        for (auto& f : m.finits)
            if (f) finits.push_back(f.get());
        processed++;
        if ((mk == 1 || mk == 2) && body) {
            HEnv env(ctx, &cls, cls.name);
            env.methodVars = buildMethodVars(ctx, cls, m);
            env.outerMembers = outerMemberNames(ctx, cls.name);
            if (mk == 1 && !m.ty.empty() && !m.ty.isVar) {
                env.hasRet = true;
                env.methodRet = m.ty;
            }
            env.hasEnclosingInstance = !hasMod(m.mods, "static");
            for (const auto& field : chainInstanceFields(ctx, cls.name)) env.outerFieldPaths[field] = "";
            for (const auto& method : chainInstanceMethods(ctx, cls.name)) env.outerMethodPaths[method] = "";
            hoistInStmt(*body, env);
        }
        for (Expr* f : finits) {
            HEnv env(ctx, &cls, cls.name);
            env.outerMembers = outerMemberNames(ctx, cls.name);
            env.hasEnclosingInstance = !hasMod(m.mods, "static");
            for (const auto& field : chainInstanceFields(ctx, cls.name)) env.outerFieldPaths[field] = "";
            for (const auto& method : chainInstanceMethods(ctx, cls.name)) env.outerMethodPaths[method] = "";
            hoistInExpr(*f, env);
        }
        if (mk == 4 && nested) hoistInDecl(ctx, *nested);
        if (mk == 3 && body) {
            HEnv env(ctx, &cls, cls.name);
            env.outerMembers = outerMemberNames(ctx, cls.name);
            env.hasEnclosingInstance = !m.isStaticInit;
            for (const auto& field : chainInstanceFields(ctx, cls.name)) env.outerFieldPaths[field] = "";
            for (const auto& method : chainInstanceMethods(ctx, cls.name)) env.outerMethodPaths[method] = "";
            hoistInStmt(*body, env);
        }
    }
}

void hoistUnit(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) hoistInDecl(ctx, *d);
    for (auto& d : u.types) {
        for (auto& ec : d->consts) {
            if (!ec.body) continue;
            Decl& eb = *ec.body;
            size_t processed = 0;
            while (processed < eb.members.size()) {
                Member& m = eb.members[processed];
                Stmt* body = m.body.get();
                processed++;
                if (body) {
                    HEnv env(ctx, &eb, eb.name);
                    env.outerMembers = outerMemberNames(ctx, eb.name);
                    hoistInStmt(*body, env);
                }
            }
        }
    }
}


std::unique_ptr<Expr> cloneExpr(const Expr& e) {
    auto c = std::make_unique<Expr>();
    c->k = e.k;
    c->op = e.op;
    c->name = e.name;
    c->val = e.val;
    c->lt = e.lt;
    c->rawLit = e.rawLit;
    c->postfix = e.postfix;
    c->diamond = e.diamond;
    c->implArr = e.implArr;
    c->ty = e.ty;
    c->lbodyIsBlock = e.lbodyIsBlock;
    c->patName = e.patName;
    c->patComps = e.patComps;
    c->targs = e.targs;
    c->line = e.line;
    if (e.obj) c->obj = cloneExpr(*e.obj);
    if (e.a) c->a = cloneExpr(*e.a);
    if (e.b) c->b = cloneExpr(*e.b);
    if (e.c) c->c = cloneExpr(*e.c);
    if (e.lbody) c->lbody = cloneExpr(*e.lbody);
    for (auto& x : e.args)
        if (x) c->args.push_back(cloneExpr(*x));
    for (auto& x : e.dims)
        if (x) c->dims.push_back(cloneExpr(*x));
    for (auto& p : e.lparams) c->lparams.push_back(p);
    return c;
}

void substParamExpr(Expr& e, const std::string& param, const Expr& rep) {
    if (e.k == EK::Ident && e.name == param) {
        std::unique_ptr<Expr> cloned = cloneExpr(rep);
        e = std::move(*cloned);
        return;
    }
    if (e.obj) substParamExpr(*e.obj, param, rep);
    if (e.a) substParamExpr(*e.a, param, rep);
    if (e.b) substParamExpr(*e.b, param, rep);
    if (e.c) substParamExpr(*e.c, param, rep);
    if (e.lbody) substParamExpr(*e.lbody, param, rep);
    for (auto& x : e.args)
        if (x) substParamExpr(*x, param, rep);
    for (auto& x : e.dims)
        if (x) substParamExpr(*x, param, rep);
}
}
