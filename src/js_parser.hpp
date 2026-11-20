#pragma once
#include "js_base.hpp"
namespace js {
struct Parser {
    std::vector<Token> ts;
    size_t p = 0;
    Ctx& ctx;

    explicit Parser(std::vector<Token> toks, Ctx& c) : ts(std::move(toks)), ctx(c) {}

    const Token& cur(size_t off = 0) const {
        size_t k = p + off;
        return k < ts.size() ? ts[k] : ts.back();
    }
    TT curT(size_t off = 0) const { return cur(off).t; }
    std::string curS(size_t off = 0) const { return cur(off).s; }
    int curLine() const { return cur().line; }

    bool isKw(const std::string& k, size_t off = 0) const { return curT(off) == TT::Kw && curS(off) == k; }
    bool isP(const std::string& s, size_t off = 0) const { return curT(off) == TT::Punct && curS(off) == s; }
    bool isId(const std::string& s, size_t off = 0) const { return curT(off) == TT::Id && curS(off) == s; }
    bool isIdAny(size_t off = 0) const { return curT(off) == TT::Id; }

    void adv() {
        if (p + 1 < ts.size()) p++;
        else p = ts.size() - 1;
    }

    bool eatKw(const std::string& k) {
        if (isKw(k)) {
            adv();
            return true;
        }
        return false;
    }
    bool eatP(const std::string& s) {
        if (isP(s)) {
            adv();
            return true;
        }
        return false;
    }
    bool eatId(const std::string& s) {
        if (isId(s)) {
            adv();
            return true;
        }
        return false;
    }
    bool expectP(const std::string& s) {
        if (eatP(s)) return true;
        ctx.error(curLine(), "expected '" + s + "' but found '" + curS() + "'");
        return false;
    }
    std::string expectId() {
        if (isIdAny()) {
            std::string s = curS();
            adv();
            return s;
        }
        ctx.error(curLine(), "expected identifier but found '" + curS() + "'");
        return "_";
    }

    void syncResync() {
        int depth = 0;
        for (;;) {
            TT t = curT();
            const std::string& s = curS();
            if (t == TT::End) return;
            if (depth == 0) {
                if (s == ";") {
                    adv();
                    return;
                }
                if (s == "}") {
                    adv();
                    return;
                }
            }
            if (s == "{" || s == "(" || s == "[") depth++;
            if (s == "}" || s == ")" || s == "]") {
                if (depth > 0) depth--;
                else return;
            }
            adv();
        }
    }

    static const std::set<std::string>& modKws() {
        static const std::set<std::string> m = {
            "public", "private", "protected", "static", "final", "abstract", "native",
            "synchronized", "transient", "volatile", "strictfp", "const", "default"};
        return m;
    }
    static bool isAnnStart(size_t off, const Parser& self) {
        return self.curT(off) == TT::Punct && self.curS(off) == "@";
    }

    Ann parseAnn() {
        Ann a;
        a.line = curLine();
        adv();
        a.name = expectId();
        while (isP(".") && isIdAny(1)) {
            adv();
            a.name += "." + expectId();
        }
        if (isP("(")) {
            a.hasArgs = true;
            adv();
            while (!isP(")") && curT() != TT::End) {
                if (isIdAny() && isP("=", 1)) {
                    std::string nm = expectId();
                    adv();
                    a.args.push_back({nm, parseExpr()});
                } else {
                    a.pargs.push_back(parseExpr());
                }
                if (!eatP(",")) break;
            }
            expectP(")");
        }
        return a;
    }

    bool parseMods(std::vector<std::string>& mods, std::vector<Ann>& anns) {
        bool any = false;
        for (;;) {
            if (curT() == TT::Punct && curS() == "@" && !isKw("interface", 1)) {
                anns.push_back(parseAnn());
                any = true;
                continue;
            }
            if (curT() == TT::Kw && modKws().count(curS()) && !isKw("default")) {
                mods.push_back(curS());
                adv();
                any = true;
                continue;
            }
            if (isKw("default")) {
                mods.push_back("default");
                adv();
                any = true;
                continue;
            }
            if ((curT() == TT::Kw || curT() == TT::Id) &&
                (curS() == "sealed" || curS() == "non-sealed")) {
                adv();
                any = true;
                continue;
            }
            break;
        }
        return any;
    }

    bool eatGenericClose() {
        if (isP(">")) {
            adv();
            return true;
        }
        if (isP(">>")) {
            ts[p].s = ">";
            return true;
        }
        if (isP(">>>")) {
            ts[p].s = ">>";
            return true;
        }
        return false;
    }

    std::vector<TP> parseTypeParams() {
        std::vector<TP> out;
        if (!eatP("<")) return out;
        for (;;) {
            TP tp;
            if (curT() == TT::Id) tp.name = expectId();
            else tp.name = "_";
            if (eatKw("extends")) {
                tp.hasBound = true;
                tp.bound = parseTypeRef(true, false);
                while (eatP("&")) {
                    TypeRef extra = parseTypeRef(true, false);
                    if (tp.bound.name.empty()) tp.bound = extra;
                }
            }
            out.push_back(tp);
            if (!eatP(",")) break;
        }
        if (!eatGenericClose()) expectP(">");
        return out;
    }

    TypeRef parseTypeRef(bool allowWild = true, bool allowVar = true) {
        TypeRef t;
        if (allowWild && isP("?")) {
            t.wildcard = true;
            adv();
            if (eatKw("extends") || eatKw("super")) {
                TypeRef b = parseTypeRef(false, false);
                t.name = b.name;
                t.args = b.args;
                t.dims = b.dims;
            }
        } else {
            if (isKw("var")) {
                t.isVar = true;
                t.name = "var";
                adv();
            } else if (curT() == TT::Kw && primitiveTypes().count(curS())) {
                t.name = curS();
                adv();
            } else if (isIdAny()) {
                t.name = expectId();
                while (isP(".") && (isIdAny(1) || (isKw("class", 1)))) {
                    if (!isIdAny(1)) break;
                    adv();
                    t.name += "." + expectId();
                }
                if (isP("<")) {
                    adv();
                    t.diamond = false;
                    for (;;) {
                        if (isP(">") && t.args.empty()) {
                            t.diamond = true;
                            break;
                        }
                        t.args.push_back(parseTypeRef(true, true));
                        if (!eatP(",")) break;
                    }
                    if (!eatGenericClose()) expectP(">");
                }
            } else {
                ctx.error(curLine(), "expected type but found '" + curS() + "'");
                t.name = "object";
            }
        }
        while (isP("[") && isP("]", 1)) {
            adv();
            adv();
            t.dims++;
        }
        (void)allowVar;
        return t;
    }

    std::unique_ptr<Unit> parseUnit() {
        auto u = std::make_unique<Unit>();
        u->line = curLine();
        if (curT() == TT::Kw && curS() == "package") {
            adv();
            u->package = expectId();
            while (eatP(".")) u->package += "." + expectId();
            expectP(";");
        }
        while (curT() == TT::Kw && curS() == "import") {
            adv();
            bool stat = eatKw("static");
            std::string path = expectId();
            bool wild = false;
            while (isP(".")) {
                adv();
                if (eatP("*")) {
                    wild = true;
                    break;
                }
                if (isKw("class")) break;
                path += "." + expectId();
            }
            expectP(";");
            u->imports.push_back({wild ? path + ".*" : path, stat});
        }
        while (curT() != TT::End) {
            if (isP(";") || isP("}")) {
                adv();
                continue;
            }
            size_t before = p;
            auto d = parseTypeDecl();
            if (d) u->types.push_back(std::move(d));
            if (p == before) adv();
        }
        return u;
    }

    bool atTypeStart() const {
        if (curT() == TT::Kw && (curS() == "class" || curS() == "interface" || curS() == "enum" || curS() == "record"))
            return true;
        if (curT() == TT::Punct && curS() == "@" && isKw("interface", 1)) return true;
        return false;
    }

    std::unique_ptr<Decl> parseTypeDecl() {
        std::vector<std::string> mods;
        std::vector<Ann> anns;
        parseMods(mods, anns);
        int line = curLine();
        std::unique_ptr<Decl> d = std::make_unique<Decl>();
        d->mods = mods;
        d->anns = std::move(anns);
        d->line = line;
        if (curT() == TT::Punct && curS() == "@" && isKw("interface", 1)) {
            adv();
            adv();
            d->dk = 4;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            parseInheritance(*d, true);
            parseTypeBody(*d, true, false, nullptr);
            return d;
        }
        if (isKw("interface")) {
            adv();
            d->dk = 1;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            parseInheritance(*d, true);
            parseTypeBody(*d, true, false, nullptr);
            return d;
        }
        if (isKw("class")) {
            adv();
            d->dk = 0;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            parseInheritance(*d, false);
            parseTypeBody(*d, false, false, nullptr);
            return d;
        }
        if (isKw("enum")) {
            adv();
            d->dk = 2;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            if (eatKw("implements")) {
                d->impls.push_back(parseTypeRef());
                while (eatP(",")) d->impls.push_back(parseTypeRef());
            }
            parseEnumBody(*d);
            return d;
        }
        if (isKw("record")) {
            adv();
            d->dk = 3;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            auto comps = parseParams(nullptr);
            if (eatKw("implements")) {
                d->impls.push_back(parseTypeRef());
                while (eatP(",")) d->impls.push_back(parseTypeRef());
            }
            parseTypeBody(*d, false, true, &comps);
            return d;
        }
        ctx.error(line, "expected type declaration but found '" + curS() + "'");
        syncResync();
        return nullptr;
    }

    void parseInheritance(Decl& d, bool iface) {
        if (eatKw("extends")) {
            d.extends = parseTypeRef();
            if (iface) {
                while (eatP(",")) {
                    TypeRef t = parseTypeRef();
                    d.impls.push_back(t);
                }
            }
        }
        if (eatKw("implements")) {
            d.impls.push_back(parseTypeRef());
            while (eatP(",")) d.impls.push_back(parseTypeRef());
        }
        if (eatKw("permits")) {
            parseTypeRef();
            while (eatP(",")) parseTypeRef();
        }
    }

    void parseTypeBody(Decl& d, bool isIface, bool isRecord, std::vector<std::pair<TypeRef, std::string>>* comps) {
        expectP("{");
        if (isRecord && comps) {
            Member m;
            m.mk = 2;
            m.ty = TypeRef();
            m.params = *comps;
            m.mods.push_back("__recordctor");
            m.body = std::make_unique<Stmt>();
            m.body->k = SK::Block;
            m.body->line = d.line;
            d.members.push_back(std::move(m));
        }
        while (!isP("}") && curT() != TT::End) {
            if (isP(";")) {
                adv();
                continue;
            }
            size_t before = p;
            parseMember(d, isIface, isRecord);
            if (p == before) adv();
        }
        expectP("}");
    }

    void parseEnumBody(Decl& d) {
        expectP("{");
        while (curT() == TT::Id && (isP(",", 1) || isP("(", 1) || isP("{", 1) || isP(";", 1) || isP("}", 1))) {
            EnumConst ec;
            ec.line = curLine();
            ec.name = expectId();
            if (isP("(")) {
                adv();
                while (!isP(")") && curT() != TT::End) {
                    ec.args.push_back(parseExpr());
                    if (!eatP(",")) break;
                }
                expectP(")");
            }
            if (isP("{")) {
                auto body = std::make_unique<Decl>();
                body->dk = 0;
                body->name = d.name + "_" + ec.name;
                body->mods.push_back("private");
                body->mods.push_back("final");
                std::vector<std::pair<TypeRef, std::string>> none;
                parseTypeBody(*body, false, false, &none);
                ec.body = std::move(body);
            }
            d.consts.push_back(std::move(ec));
            if (!eatP(",")) break;
        }
        if (eatP(";")) {
            while (!isP("}") && curT() != TT::End) {
                if (isP(";")) {
                    adv();
                    continue;
                }
                size_t before = p;
                parseMember(d, false, false);
                if (p == before) adv();
            }
        }
        expectP("}");
    }

    void parseMember(Decl& d, bool isIface, bool isRecord) {
        std::vector<std::string> mods;
        std::vector<Ann> anns;
        int line = curLine();
        parseMods(mods, anns);
        if (isP("{")) {
            Member m;
            m.mk = 3;
            m.mods = mods;
            m.anns = std::move(anns);
            m.line = line;
            if (std::find(m.mods.begin(), m.mods.end(), "static") != m.mods.end()) {
                m.isStaticInit = true;
                m.mods.erase(std::remove(m.mods.begin(), m.mods.end(), "static"), m.mods.end());
            }
            m.body = parseBlock();
            d.members.push_back(std::move(m));
            return;
        }
        if (atTypeStart()) {
            auto nd = parseTypeDecl();
            if (nd) {
                nd->mods.insert(nd->mods.begin(), mods.begin(), mods.end());
                nd->anns.insert(nd->anns.begin(), std::make_move_iterator(anns.begin()),
                                std::make_move_iterator(anns.end()));
                Member m;
                m.mk = 4;
                m.mods = mods;
                m.line = line;
                m.nested = std::move(nd);
                d.members.push_back(std::move(m));
            }
            return;
        }
        bool ctorGeneric = isP("<") && isId(d.name, 1);
        if (getenv("JS_DEBUG")) fprintf(stderr, "[member] d=%s cur='%s' next='%s' idmatch=%d paren=%d\n", d.name.c_str(), curS().c_str(), curS(1).c_str(), (int)isId(d.name), (int)isP("(", 1));
        if (isRecord && isId(d.name) && isP("{", 1)) {
            Member m;
            m.mk = 2;
            m.mods = mods;
            m.mods.push_back("__recordctor");
            m.anns = std::move(anns);
            m.line = line;
            m.compactCtor = true;
            m.name = d.name;
            m.ty.name = d.name;
            adv();
            m.body = parseBlock();
            d.members.push_back(std::move(m));
            return;
        }
        if ((isId(d.name) && isP("(", 1)) || ctorGeneric) {
            Member m;
            m.mk = 2;
            m.mods = mods;
            m.anns = std::move(anns);
            m.line = line;
            m.name = d.name;
            m.ty.name = d.name;
            if (ctorGeneric) m.tparams = parseTypeParams();
            else adv();
            bool va = false;
            m.params = parseParams(&va);
            m.varargs = va;
            parseThrows(m);
            if (isP("{")) {
                m.body = parseBlock();
            } else {
                expectP(";");
            }
            d.members.push_back(std::move(m));
            return;
        }
        if (isIface && curT() == TT::Kw && curS() == "void" && isIdAny(1) && isP("(", 2)) {
            TypeRef rt;
            rt.name = "void";
            Member m;
            m.mk = 1;
            m.mods = mods;
            m.anns = std::move(anns);
            m.ty = rt;
            m.line = line;
            adv();
            m.name = expectId();
            bool va = false;
            m.params = parseParams(&va);
            m.varargs = va;
            parseThrows(m);
            if (isP("{")) m.body = parseBlock();
            else expectP(";");
            d.members.push_back(std::move(m));
            return;
        }
        Member m;
        m.mods = mods;
        m.anns = std::move(anns);
        m.line = line;
        if (isP("<")) m.tparams = parseTypeParams();
        m.ty = parseTypeRef(true, true);
        bool isMethod = false;
        std::string name;
        if (curT() == TT::Kw && primitiveTypes().count(curS()) && isIdAny(1)) {
            name = curS(1);
            isMethod = true;
        } else if (curT() == TT::Kw && curS() == "void" && isIdAny(1)) {
            name = curS(1);
            isMethod = true;
        } else if (isIdAny()) {
            name = curS();
            if (isP("(", 1)) isMethod = true;
            else if (isP("<", 1)) isMethod = true;
            else adv();
        } else {
            ctx.error(line, "expected member but found '" + curS() + "'");
            syncResync();
            return;
        }
        if (isMethod) {
            adv();
            m.mk = 1;
            m.name = name;
            if (isP("<")) m.tparams = parseTypeParams();
            bool va = false;
            m.params = parseParams(&va);
            m.varargs = va;
            parseThrows(m);
            if (isP("{")) {
                m.body = parseBlock();
            } else if (isKw("default")) {
                adv();
                auto blk = std::make_unique<Stmt>();
                blk->k = SK::Block;
                blk->line = curLine();
                auto ret = std::make_unique<Stmt>();
                ret->k = SK::Return;
                ret->line = curLine();
                ret->e = parseExpr();
                blk->block.push_back(std::move(ret));
                m.body = std::move(blk);
                expectP(";");
            } else {
                expectP(";");
            }
            d.members.push_back(std::move(m));
            return;
        }
        m.mk = 0;
        for (;;) {
            m.names.push_back(name);
            int extra = 0;
            while (isP("[") && isP("]", 1)) {
                adv();
                adv();
                extra++;
            }
            m.fdims.push_back(extra);
            if (eatP("=")) {
                m.finits.push_back(parseVarInit());
            } else {
                m.finits.push_back(nullptr);
            }
            if (eatP(",")) {
                if (curT() == TT::Kw && primitiveTypes().count(curS()) && isIdAny(1)) {
                    name = curS(1);
                    adv();
                    adv();
                } else if (curT() == TT::Kw && curS() == "void" && isIdAny(1)) {
                    name = curS(1);
                    adv();
                    adv();
                } else {
                    name = expectId();
                }
                continue;
            }
            break;
        }
        expectP(";");
        d.members.push_back(std::move(m));
        (void)isRecord;
    }

    void parseThrows(Member& m) {
        if (eatKw("throws")) {
            m.throws.push_back(parseTypeRef());
            while (eatP(",")) m.throws.push_back(parseTypeRef());
        }
    }

    bool tryLocalVarHeader(TypeRef* outTy, std::string* outName);
    std::unique_ptr<Expr> parseAssign();
    std::vector<std::pair<TypeRef, std::string>> parseParams(bool* varargs);
    std::unique_ptr<Expr> parseVarInit();
    std::unique_ptr<Stmt> parseBlock();
    std::vector<std::unique_ptr<Stmt>> parseStmtListUntil(const std::string& closer, bool consume);
    std::unique_ptr<Stmt> parseStmt();
    std::vector<std::unique_ptr<Stmt>> parseCaseBody();
    std::unique_ptr<Stmt> parseLocalVar(bool allowFinal);
    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseExprNoLambda();
    std::unique_ptr<Expr> parseTernary();
    std::unique_ptr<Expr> parseBinary(int minPrec);
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfixExpr();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parseNew();
    std::unique_ptr<Expr> parseLambdaOrExpr();
    std::unique_ptr<Expr> parseParOrLambda();
    bool looksLikeLambda(size_t off) const;
    int probeParenType(size_t off, bool* primOut) const;
    bool parseGenericTypeArgs(std::vector<TypeRef>& out);
};
std::vector<std::pair<TypeRef, std::string>> Parser::parseParams(bool* varargs) {
    std::vector<std::pair<TypeRef, std::string>> out;
    if (getenv("JS_DEBUG")) fprintf(stderr, "[params] enter cur='%s'\n", curS().c_str());
    expectP("(");
    if (!isP(")")) {
        for (;;) {
            while (isKw("final")) adv();
            while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
            TypeRef t = parseTypeRef(true, false);
            if (isP("...")) {
                adv();
                if (varargs) *varargs = true;
            }
            std::string nm = expectId();
            int extra = 0;
            while (isP("[") && isP("]", 1)) {
                adv();
                adv();
                extra++;
            }
            t.dims += extra;
            out.push_back({t, nm});
            if (!eatP(",")) break;
        }
    }
    expectP(")");
    return out;
}

std::unique_ptr<Expr> Parser::parseVarInit() {
    if (isP("{")) {
        auto e = std::make_unique<Expr>();
        e->k = EK::NewArr;
        e->implArr = true;
        e->line = curLine();
        adv();
        while (!isP("}") && curT() != TT::End) {
            e->args.push_back(parseVarInit());
            if (!eatP(",")) break;
        }
        expectP("}");
        return e;
    }
    return parseExpr();
}

std::unique_ptr<Stmt> Parser::parseBlock() {
    auto s = std::make_unique<Stmt>();
    s->k = SK::Block;
    s->line = curLine();
    expectP("{");
    s->block = parseStmtListUntil("}", true);
    return s;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseStmtListUntil(const std::string& closer, bool consume) {
    std::vector<std::unique_ptr<Stmt>> out;
    while (!isP(closer) && curT() != TT::End) {
        if (isP(";")) {
            adv();
            continue;
        }
        auto st = parseStmt();
        if (st) out.push_back(std::move(st));
        else break;
    }
    if (consume) expectP(closer);
    return out;
}

bool Parser::tryLocalVarHeader(TypeRef* outTy, std::string* outName) {
    size_t save = p;
    while (isKw("final")) adv();
    while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
    bool ok = true;
    TypeRef t;
    if (curT() == TT::Kw && primitiveTypes().count(curS())) {
        t = parseTypeRef(false, false);
    } else if (isIdAny() || isKw("var")) {
        t = parseTypeRef(true, true);
    } else {
        ok = false;
    }
    if (ok) {
        if (isIdAny()) {
            std::string nm = curS();
            size_t save2 = p;
            adv();
            if (isP("=") || isP(";") || isP(",") || isP(":")) {
                if (outTy) *outTy = t;
                if (outName) *outName = nm;
                p = save2;
                return true;
            }
            p = save2;
        }
    }
    p = save;
    return false;
}

std::unique_ptr<Stmt> Parser::parseLocalVar(bool allowFinal) {
    auto s = std::make_unique<Stmt>();
    s->k = SK::LocalVar;
    s->line = curLine();
    (void)allowFinal;
    while (isKw("final")) adv();
    while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
    s->ty = parseTypeRef(true, true);
    for (;;) {
        std::string nm = isIdAny() ? expectId() : expectId();
        int extra = 0;
        while (isP("[") && isP("]", 1)) {
            adv();
            adv();
            extra++;
        }
        s->names.push_back(nm);
        s->dims.push_back(extra);
        if (eatP("=")) {
            s->inits.push_back(parseVarInit());
        } else {
            s->inits.push_back(nullptr);
        }
        if (!eatP(",")) break;
    }
    expectP(";");
    return s;
}

std::unique_ptr<Stmt> Parser::parseStmt() {
    int line = curLine();
    while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
    auto typeAhead = [&](size_t off) {
        return isKw("class", off) || isKw("interface", off) || isKw("enum", off) ||
               isKw("record", off) || (isP("@", off) && isKw("interface", off + 1));
    };
    bool localTypeStart = typeAhead(0) ||
                          ((isKw("final") || isKw("abstract")) && typeAhead(1));
    if (localTypeStart) {
        auto d = parseTypeDecl();
        auto s = std::make_unique<Stmt>();
        s->line = line;
        if (d) {
            s->k = SK::LocalClass;
            s->lclass = std::move(d);
        } else {
            s->k = SK::Empty;
        }
        return s;
    }
    if (isP("{")) return parseBlock();
    if (isP(";")) {
        auto s = std::make_unique<Stmt>();
        s->k = SK::Empty;
        s->line = line;
        adv();
        return s;
    }
    if (isIdAny() && isP(":", 1) && curS() != "default") {
        auto s = std::make_unique<Stmt>();
        s->k = SK::Label;
        s->line = line;
        s->label = expectId();
        adv();
        s->body = parseStmt();
        return s;
    }
    if (isKw("if")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::If;
        s->line = line;
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        s->body = parseStmt();
        if (eatKw("else")) s->body2 = parseStmt();
        return s;
    }
    if (isKw("while")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::While;
        s->line = line;
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        s->body = parseStmt();
        return s;
    }
    if (isKw("do")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::DoWhile;
        s->line = line;
        s->body = parseStmt();
        if (!eatKw("while")) ctx.error(curLine(), "expected 'while' after do-block");
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        expectP(";");
        return s;
    }
    if (isKw("for")) {
        adv();
        expectP("(");
        bool enh = false;
        {
            size_t save = p;
            while (isKw("final")) adv();
            if (isIdAny() || isKw("var") || (curT() == TT::Kw && primitiveTypes().count(curS()))) {
                TypeRef t = parseTypeRef(true, true);
                if ((isIdAny() || isKw("yield")) && isP(":", 1)) enh = true;
                (void)t;
            }
            p = save;
        }
        auto s = std::make_unique<Stmt>();
        s->line = line;
        if (enh) {
            s->k = SK::Foreach;
            while (isKw("final")) adv();
            s->ty = parseTypeRef(true, true);
            s->names.push_back(isIdAny() ? expectId() : "_");
            expectP(":");
            s->e = parseExpr();
            expectP(")");
            s->body = parseStmt();
            return s;
        }
        s->k = SK::For;
        if (!isP(";")) {
            size_t save = p;
            TypeRef vty;
            std::string vnm;
            bool decl = tryLocalVarHeader(&vty, &vnm);
            if (decl) {
                s->ty = vty;
                for (;;) {
                    std::string nm = expectId();
                    int extra = 0;
                    while (isP("[") && isP("]", 1)) {
                        adv();
                        adv();
                        extra++;
                    }
                    s->names.push_back(nm);
                    s->dims.push_back(extra);
                    if (eatP("=")) s->inits.push_back(parseVarInit());
                    else s->inits.push_back(nullptr);
                    if (!eatP(",")) break;
                }
            } else {
                p = save;
                for (;;) {
                    s->initExprs.push_back(parseExpr());
                    if (!eatP(",")) break;
                }
            }
        }
        expectP(";");
        if (!isP(";")) s->e = parseExpr();
        expectP(";");
        if (!isP(")")) {
            for (;;) {
                s->updateExprs.push_back(parseExpr());
                if (!eatP(",")) break;
            }
        }
        expectP(")");
        s->body = parseStmt();
        return s;
    }
    if (isKw("return")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Return;
        s->line = line;
        if (!isP(";")) s->e = parseExpr();
        expectP(";");
        return s;
    }
    if (isKw("yield")) {
        if (isP(";", 1) || isIdAny(1) || curT(1) == TT::Int || curT(1) == TT::Str ||
            curT(1) == TT::Chr || curT(1) == TT::Flt || isP("(", 1)) {
            adv();
            auto s = std::make_unique<Stmt>();
            s->k = SK::Return;
            s->isYield = true;
            s->line = line;
            if (!isP(";")) s->e = parseExpr();
            expectP(";");
            return s;
        }
    }
    if (isKw("throw")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Throw;
        s->line = line;
        s->e = parseExpr();
        expectP(";");
        return s;
    }
    if (isKw("break")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Break;
        s->line = line;
        if (isIdAny()) s->label = expectId();
        expectP(";");
        return s;
    }
    if (isKw("continue")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Continue;
        s->line = line;
        if (isIdAny()) s->label = expectId();
        expectP(";");
        return s;
    }
    if (isKw("assert")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Assert;
        s->line = line;
        s->e = parseExpr();
        if (eatP(":")) s->e2 = parseExpr();
        expectP(";");
        return s;
    }
    if (isKw("synchronized")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Sync;
        s->line = line;
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        s->body = parseStmt();
        return s;
    }
    if (isKw("switch")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Switch;
        s->line = line;
        expectP("(");
        s->subject = parseExpr();
        expectP(")");
        expectP("{");
        bool anyArrow = false;
        while (!isP("}") && curT() != TT::End) {
            CaseArm arm;
            arm.line = curLine();
            if (eatP(";")) continue;
            if (isKw("default")) {
                adv();
                arm.isDefault = true;
            } else if (eatKw("case")) {
                for (;;) {
                    if (curT() == TT::Null) {
                        adv();
                        arm.nullLabel = true;
                    } else if (isP("(")) {
                        adv();
                        arm.labels.push_back(parseExpr());
                        expectP(")");
                    } else {
                        size_t save = p;
                        bool patOk = false;
                        TypeRef pt;
                        std::string pn;
                        if ((isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS()))) &&
                            (isIdAny(1) && curS(1) != "when")) {
                            pt = parseTypeRef(false, false);
                            if (isIdAny() && curS() != "when") {
                                pn = expectId();
                                patOk = true;
                            }
                        }
                        if (patOk) {
                            arm.patLabels.push_back(pt);
                            arm.patName = pn;
                        } else {
                            p = save;
                            arm.labels.push_back(parseExprNoLambda());
                        }
                    }
                    if (eatP(",")) continue;
                    break;
                }
            } else {
                ctx.error(curLine(), "expected 'case' or 'default' in switch");
                break;
            }
            if ((isKw("when") || isId("when"))) {
                adv();
                arm.guard = parseExpr();
            }
            if (eatP("->")) {
                arm.arrow = true;
                anyArrow = true;
                if (isP("{")) {
                    arm.isBlockVal = true;
                    adv();
                    arm.body = parseStmtListUntil("}", true);
                } else if (isKw("throw")) {
                    adv();
                    auto th = std::make_unique<Stmt>();
                    th->k = SK::Throw;
                    th->line = curLine();
                    th->e = parseExpr();
                    expectP(";");
                    arm.body.push_back(std::move(th));
                } else {
                    arm.value = parseExpr();
                    expectP(";");
                }
            } else {
                expectP(":");
                arm.body = parseCaseBody();
            }
            s->arms.push_back(std::move(arm));
        }
        expectP("}");
        s->arrowSw = anyArrow;
        return s;
    }
    if (isKw("try")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Try;
        s->line = line;
        if (isP("(")) {
            adv();
            while (!isP(")") && curT() != TT::End) {
                while (isKw("final")) adv();
                while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
                TypeRef rt = parseTypeRef(true, true);
                std::string rn = isIdAny() ? expectId() : "_";
                expectP("=");
                s->resources.push_back({rt, rn});
                s->resInits.push_back(parseExpr());
                if (!eatP(";")) break;
            }
            expectP(")");
        }
        s->body = parseBlock();
        while (isKw("catch")) {
            adv();
            Stmt::Catch c;
            expectP("(");
            for (;;) {
                c.types.push_back(parseTypeRef(true, false));
                if (!eatP("|")) break;
            }
            while (isKw("final")) adv();
            c.var = expectId();
            expectP(")");
            c.body = parseBlock();
            s->catches.push_back(std::move(c));
        }
        if (eatKw("finally")) s->fin = parseBlock();
        return s;
    }
    {
        TypeRef vty;
        std::string vnm;
        size_t saveLV = p;
        if (tryLocalVarHeader(&vty, &vnm)) {
            auto s = std::make_unique<Stmt>();
            s->k = SK::LocalVar;
            s->line = line;
            s->ty = vty;
            for (;;) {
                std::string nm = expectId();
                int extra = 0;
                while (isP("[") && isP("]", 1)) {
                    adv();
                    adv();
                    extra++;
                }
                s->names.push_back(nm);
                s->dims.push_back(extra);
                if (eatP("=")) s->inits.push_back(parseVarInit());
                else s->inits.push_back(nullptr);
                if (!eatP(",")) break;
            }
            expectP(";");
            return s;
        }
        p = saveLV;
    }
    auto s = std::make_unique<Stmt>();
    s->k = SK::ExprStmt;
    s->line = line;
    s->e = parseExpr();
    expectP(";");
    return s;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseCaseBody() {
    std::vector<std::unique_ptr<Stmt>> out;
    for (;;) {
        if (isP("}") || curT() == TT::End) break;
        if (isKw("case") || isKw("default")) break;
        if (isP(";")) {
            adv();
            continue;
        }
        auto st = parseStmt();
        if (!st) break;
        out.push_back(std::move(st));
    }
    return out;
}
bool Parser::looksLikeLambda(size_t off) const {
    if (curT(off) == TT::Id && curS(off) != "var" && isP("->", off + 1)) return true;
    if (isP("(", off)) {
        int depth = 0;
        size_t k = p + off;
        while (k < ts.size()) {
            const Token& tk = ts[k];
            if (tk.t == TT::Punct) {
                if (tk.s == "(" || tk.s == "[" || tk.s == "{") depth++;
                else if (tk.s == ")" || tk.s == "]" || tk.s == "}") {
                    depth--;
                    if (depth == 0) {
                        size_t n = k + 1;
                        return n < ts.size() && ts[n].t == TT::Punct && ts[n].s == "->";
                    }
                }
            } else if (tk.t == TT::End) return false;
            k++;
        }
        return false;
    }
    return false;
}

std::unique_ptr<Expr> Parser::parseLambdaOrExpr() {
    if (looksLikeLambda(0)) {
        auto e = std::make_unique<Expr>();
        e->k = EK::Lambda;
        e->line = curLine();
        if (isP("(")) {
            adv();
            while (!isP(")") && curT() != TT::End) {
                while (isKw("final")) adv();
                bool typed = false;
                TypeRef t;
                if (isKw("var") && isIdAny(1)) {
                    t.isVar = true;
                    t.name = "var";
                    adv();
                    typed = true;
                } else if ((isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS()))) &&
                           (isIdAny(1) || isP("...", 1))) {
                    t = parseTypeRef(false, false);
                    typed = true;
                }
                if (typed && isP("...")) adv();
                std::string nm = expectId();
                e->lparams.push_back({t, nm});
                if (!eatP(",")) break;
            }
            expectP(")");
        } else {
            std::string nm = expectId();
            e->lparams.push_back({TypeRef(), nm});
        }
        expectP("->");
        if (isP("{")) {
            e->lbodyIsBlock = true;
            adv();
            e->lblock = parseStmtListUntil("}", true);
        } else {
            e->lbody = parseExpr();
        }
        return e;
    }
    return parseAssign();
}

std::unique_ptr<Expr> Parser::parseExpr() {
    if (getenv("JS_DEBUG")) fprintf(stderr, "[parseExpr] at '%s' lambda=%d\n", curS().c_str(), (int)looksLikeLambda(0));
    if (isKw("switch") && isP("(", 1)) {
        auto e = std::make_unique<Expr>();
        e->k = EK::SwitchE;
        e->line = curLine();
        adv();
        expectP("(");
        e->a = parseExpr();
        expectP(")");
        expectP("{");
        while (!isP("}") && curT() != TT::End) {
            if (isP(";")) {
                adv();
                continue;
            }
            CaseArm arm;
            arm.line = curLine();
            arm.arrow = true;
            if (isKw("default")) {
                adv();
                arm.isDefault = true;
            } else {
                eatKw("case");
                for (;;) {
                    if (curT() == TT::Null) {
                        adv();
                        arm.nullLabel = true;
                    } else if (isP("(")) {
                        adv();
                        arm.labels.push_back(parseExpr());
                        expectP(")");
                    } else {
                        size_t save = p;
                        bool patOk = false;
                        TypeRef pt;
                        std::string pn;
                        if ((isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS()))) &&
                            (isIdAny(1) && curS(1) != "when")) {
                            pt = parseTypeRef(false, false);
                            if (isIdAny() && curS() != "when") {
                                pn = expectId();
                                patOk = true;
                            }
                        }
                        if (patOk) {
                            arm.patLabels.push_back(pt);
                            arm.patName = pn;
                        } else {
                            p = save;
                            arm.labels.push_back(parseExprNoLambda());
                        }
                    }
                    if (eatP(",")) continue;
                    break;
                }
            }
            if ((isKw("when") || isId("when"))) {
                adv();
                arm.guard = parseExpr();
            }
            expectP("->");
            if (isP("{")) {
                arm.isBlockVal = true;
                adv();
                arm.body = parseStmtListUntil("}", true);
            } else if (isKw("throw")) {
                adv();
                auto th = std::make_unique<Stmt>();
                th->k = SK::Throw;
                th->line = curLine();
                th->e = parseExpr();
                expectP(";");
                arm.body.push_back(std::move(th));
            } else {
                arm.value = parseExpr();
                expectP(";");
            }
            e->cases.push_back(std::move(arm));
        }
        expectP("}");
        return e;
    }
    if (looksLikeLambda(0)) return parseLambdaOrExpr();
    return parseAssign();
}

std::unique_ptr<Expr> Parser::parseExprNoLambda() { return parseAssign(); }

std::unique_ptr<Expr> Parser::parseAssign() {
    auto lhs = parseTernary();
    static const char* compound[] = {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
                                     "<<=", ">>=", ">>>=", nullptr};
    for (auto* cop : compound) {
        if (cop && isP(cop)) {
            std::string op = cop;
            adv();
            auto e = std::make_unique<Expr>();
            e->k = EK::Assign;
            e->op = op;
            e->line = lhs->line;
            e->a = std::move(lhs);
            e->b = parseExpr();
            return e;
        }
    }
    if (isP("=")) {
        adv();
        auto e = std::make_unique<Expr>();
        e->k = EK::Assign;
        e->op = "=";
        e->line = lhs->line;
        e->a = std::move(lhs);
        e->b = parseExpr();
        return e;
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseTernary() {
    auto cond = parseBinary(0);
    if (isP("?")) {
        adv();
        auto e = std::make_unique<Expr>();
        e->k = EK::Ternary;
        e->line = cond->line;
        e->a = std::move(cond);
        e->b = parseExpr();
        expectP(":");
        e->c = parseExpr();
        return e;
    }
    return cond;
}

int binPrec(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "|") return 3;
    if (op == "^") return 4;
    if (op == "&") return 5;
    if (op == "==" || op == "!=") return 6;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
    if (op == "<<" || op == ">>" || op == ">>>") return 8;
    if (op == "+" || op == "-") return 9;
    if (op == "*" || op == "/" || op == "%") return 10;
    return 0;
}

std::unique_ptr<Expr> Parser::parseBinary(int minPrec) {
    auto lhs = parseUnary();
    for (;;) {
        std::string op;
        int prec = 0;
        if (isKw("instanceof")) {
            op = "instanceof";
            prec = 7;
        } else {
            for (const char* cand : {"||", "&&", "|", "^", "&", "==", "!=", "<", ">",
                                     "<=", ">=", "<<", ">>", ">>>", "+", "-", "*", "/", "%"}) {
                if (isP(cand)) {
                    op = cand;
                    prec = binPrec(cand);
                    break;
                }
            }
        }
        if (prec == 0 || prec < minPrec) break;
        int line = curLine();
        adv();
        if (op == "instanceof") {
            auto e = std::make_unique<Expr>();
            e->k = EK::InstOf;
            e->line = line;
            e->a = std::move(lhs);
            e->ty = parseTypeRef(true, false);
            if (isIdAny()) {
                e->patName = expectId();
            } else if (isP("(")) {
                adv();
                for (;;) {
                    TypeRef ct = parseTypeRef(false, false);
                    std::string cn = expectId();
                    e->patComps.push_back({ct, cn});
                    if (!eatP(",")) break;
                }
                expectP(")");
            }
            lhs = std::move(e);
            continue;
        }
        auto rhs = parseBinary(prec + 1);
        auto e = std::make_unique<Expr>();
        e->k = EK::Bin;
        e->op = op;
        e->line = line;
        e->a = std::move(lhs);
        e->b = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

int Parser::probeParenType(size_t off, bool* primOut) const {
    size_t k = off + 1;
    int depth = 0;
    bool sawType = false;
    *primOut = false;
    while (k < ts.size()) {
        const Token& tk = ts[k];
        if (tk.t == TT::Punct) {
            if (tk.s == "<") {
                depth++;
                k++;
                continue;
            }
            if (tk.s == ">") {
                if (depth == 0) return -1;
                depth--;
                k++;
                continue;
            }
            if (depth > 0) {
                if (tk.s == "," || tk.s == "?" || tk.s == ">>" || tk.s == ">>>" || tk.s == "." ||
                    (tk.t == TT::Kw && primitiveTypes().count(tk.s))) {
                    if (tk.s == ">>" || tk.s == ">>>") depth -= (int)tk.s.size();
                    k++;
                    continue;
                }
                return -1;
            }
            if (tk.s == ")") return sawType ? (int)k : -1;
            if (tk.s == "[") {
                if (!sawType || k + 1 >= ts.size() || ts[k + 1].t != TT::Punct || ts[k + 1].s != "]")
                    return -1;
                k += 2;
                continue;
            }
            return -1;
        }
        if (tk.t == TT::Kw && primitiveTypes().count(tk.s)) {
            if (depth > 0 || sawType) return -1;
            sawType = true;
            *primOut = true;
            k++;
            continue;
        }
        if (tk.t == TT::Id) {
            if (sawType || depth > 0) return -1;
            sawType = true;
            k++;
            while (k < ts.size() && ts[k].t == TT::Punct && ts[k].s == "." && k + 1 < ts.size() &&
                   ts[k + 1].t == TT::Id) {
                k += 2;
            }
            continue;
        }
        if (tk.t == TT::Kw && (tk.s == "var")) return -1;
        return -1;
    }
    return -1;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    int line = curLine();
    if (isP("(")) {
        bool prim = false;
        int closeIdx = probeParenType(p, &prim);
        if (closeIdx > 0) {
            size_t after = (size_t)closeIdx + 1;
            bool castable = false;
            if (after < ts.size()) {
                const Token& nx = ts[after];
                if (nx.t == TT::Id || nx.t == TT::Int || nx.t == TT::Flt || nx.t == TT::Chr ||
                    nx.t == TT::Str || nx.t == TT::TBlk || nx.t == TT::Bool || nx.t == TT::Null)
                    castable = true;
                else if (nx.t == TT::Punct && (nx.s == "(" || nx.s == "!" || nx.s == "~"))
                    castable = true;
                else if (nx.t == TT::Punct && (nx.s == "++" || nx.s == "--"))
                    castable = true;
                else if (nx.t == TT::Kw)
                    castable = (nx.s != "instanceof");
                else if (nx.t == TT::Punct && (nx.s == "+" || nx.s == "-") && prim)
                    castable = true;
            }
            if (castable) {
                adv();
                auto e = std::make_unique<Expr>();
                e->k = EK::Cast;
                e->line = line;
                e->ty = parseTypeRef(true, false);
                expectP(")");
                e->a = parseUnary();
                return e;
            }
        }
    }
    static const char* pre[] = {"+", "-", "!", "~", "++", "--", nullptr};
    for (auto* op : pre) {
        if (op && isP(op)) {
            adv();
            auto e = std::make_unique<Expr>();
            e->k = EK::Un;
            e->op = op;
            e->line = line;
            e->a = parseUnary();
            return e;
        }
    }
    return parsePostfixExpr();
}

std::unique_ptr<Expr> Parser::parsePostfixExpr() {
    auto e = parsePrimary();
    if (e->k == EK::Ident && isP("(") && e->name != "super") {
        auto call = std::make_unique<Expr>();
        call->k = EK::Call;
        call->line = e->line;
        call->name = e->name;
        adv();
        while (!isP(")") && curT() != TT::End) {
            call->args.push_back(parseExpr());
            if (!eatP(",")) break;
        }
        expectP(")");
        e = std::move(call);
    }
    for (;;) {
        int line = curLine();
        if (isP(".")) {
            if (isKw("class", 1)) {
                adv();
                adv();
                auto cl = std::make_unique<Expr>();
                cl->k = EK::ClassLit;
                cl->line = line;
                cl->ty = e->k == EK::Ident ? e->ty : TypeRef();
                if (e->k == EK::Ident) cl->ty.name = e->name;
                else if (e->k == EK::Field) cl->ty.name = e->name;
                e = std::move(cl);
                continue;
            }
            if (isId("this", 1)) {
                adv();
                adv();
                auto fld = std::make_unique<Expr>();
                fld->k = EK::Field;
                fld->name = "this";
                fld->line = line;
                fld->obj = std::move(e);
                e = std::move(fld);
                continue;
            }
            if (isKw("new", 1) && isP("(", 2)) {
                adv();
                adv();
                e = parseNew();
                continue;
            }
            adv();
            if (isP("<")) {
                size_t save = p;
                std::vector<TypeRef> ta;
                bool ok = parseGenericTypeArgs(ta);
                if (ok && isIdAny() && isP("(", 1)) {
                    std::string nm = expectId();
                    auto call = std::make_unique<Expr>();
                    call->k = EK::Call;
                    call->line = line;
                    call->name = nm;
                    call->obj = std::move(e);
                    call->targs = ta;
                    adv();
                    while (!isP(")") && curT() != TT::End) {
                        call->args.push_back(parseExpr());
                        if (!eatP(",")) break;
                    }
                    expectP(")");
                    e = std::move(call);
                    continue;
                }
                p = save;
            }
            std::string nm = expectId();
            if (isP("(")) {
                auto call = std::make_unique<Expr>();
                call->k = EK::Call;
                call->line = line;
                call->name = nm;
                call->obj = std::move(e);
                adv();
                while (!isP(")") && curT() != TT::End) {
                    call->args.push_back(parseExpr());
                    if (!eatP(",")) break;
                }
                expectP(")");
                e = std::move(call);
                continue;
            }
            auto fld = std::make_unique<Expr>();
            fld->k = EK::Field;
            fld->name = nm;
            fld->line = line;
            fld->obj = std::move(e);
            e = std::move(fld);
            continue;
        }
        if (isP("[")) {
            adv();
            auto idx = std::make_unique<Expr>();
            idx->k = EK::ArrIdx;
            idx->line = line;
            idx->a = std::move(e);
            idx->b = parseExpr();
            expectP("]");
            e = std::move(idx);
            continue;
        }
        if (isP("++") || isP("--")) {
            auto po = std::make_unique<Expr>();
            po->k = EK::Post;
            po->op = curS();
            po->line = line;
            po->postfix = true;
            po->a = std::move(e);
            adv();
            e = std::move(po);
            continue;
        }
        if (isP("::")) {
            adv();
            std::string nm;
            if (isKw("new")) {
                adv();
                nm = "new";
            } else nm = expectId();
            auto mr = std::make_unique<Expr>();
            mr->k = EK::MRef;
            mr->line = line;
            mr->name = nm;
            mr->obj = std::move(e);
            e = std::move(mr);
            continue;
        }
        break;
    }
    return e;
}

bool Parser::parseGenericTypeArgs(std::vector<TypeRef>& out) {
    if (!isP("<")) return false;
    size_t save = p;
    adv();
    int depth = 1;
    bool ok = true;
    while (depth > 0) {
        if (curT() == TT::End) {
            ok = false;
            break;
        }
        if (isP("<")) {
            depth++;
        } else if (isP(">")) {
            depth--;
        } else if (isP(">>") || isP(">>>")) {
            depth -= (int)curS().size();
        } else if (isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS())) ||
                   isP(",") || isP("?") || isP(".") || isP("extends") || isP("super")) {
        } else {
            ok = false;
            break;
        }
        adv();
    }
    bool hasCallTarget = isP("(") || (isIdAny() && isP("(", 1));
    if (!ok || !hasCallTarget) {
        p = save;
        return false;
    }
    p = save;
    adv();
    depth = 1;
    while (depth > 0) {
        if (isP("<")) depth++;
        else if (isP(">")) depth--;
        else if (isP(">>") || isP(">>>")) depth -= (int)curS().size();
        else if (isP(",")) {
            adv();
            continue;
        } else if (isP("?")) {
            adv();
            if (eatKw("extends") || eatKw("super")) (void)parseTypeRef(false, false);
            continue;
        } else {
            out.push_back(parseTypeRef(false, false));
            continue;
        }
        adv();
    }
    return true;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    int line = curLine();
    TT t = curT();
    const std::string s = curS();
    if (t == TT::Int || t == TT::Flt || t == TT::Chr || t == TT::Str || t == TT::TBlk ||
        t == TT::Bool || t == TT::Null) {
        auto e = std::make_unique<Expr>();
        e->k = EK::Lit;
        e->lt = t;
        e->val = s;
        e->line = line;
        adv();
        return e;
    }
    if (isKw("this")) {
        adv();
        if (isP("(")) {
            auto call = std::make_unique<Expr>();
            call->k = EK::CtorCall;
            call->name = "this";
            call->line = line;
            adv();
            while (!isP(")") && curT() != TT::End) {
                call->args.push_back(parseExpr());
                if (!eatP(",")) break;
            }
            expectP(")");
            return call;
        }
        auto e = std::make_unique<Expr>();
        e->k = EK::This;
        e->line = line;
        return e;
    }
    if (isKw("super")) {
        adv();
        if (isP("(")) {
            auto call = std::make_unique<Expr>();
            call->k = EK::CtorCall;
            call->name = "super";
            call->line = line;
            adv();
            while (!isP(")") && curT() != TT::End) {
                call->args.push_back(parseExpr());
                if (!eatP(",")) break;
            }
            expectP(")");
            return call;
        }
        auto e = std::make_unique<Expr>();
        e->k = EK::Ident;
        e->name = "super";
        e->line = line;
        return e;
    }
    if (isKw("new")) return parseNew();
    if (isP("(")) {
        adv();
        auto e = parseExpr();
        expectP(")");
        return e;
    }
    if (t == TT::Id) {
        auto e = std::make_unique<Expr>();
        e->k = EK::Ident;
        e->name = expectId();
        e->line = line;
        return e;
    }
    ctx.error(line, "unexpected token '" + s + "' in expression");
    adv();
    auto e = std::make_unique<Expr>();
    e->k = EK::Ident;
    e->name = "_";
    e->line = line;
    return e;
}

std::unique_ptr<Expr> Parser::parseNew() {
    int line = curLine();
    adv();
    auto e = std::make_unique<Expr>();
    e->line = line;
    if (isP("[") && isP("]", 1)) {
        e->k = EK::NewArr;
        e->implArr = true;
        adv();
        adv();
        while (isP("[") && isP("]", 1)) {
            adv();
            adv();
            e->ty.dims++;
        }
        if (isP("{")) {
            adv();
            while (!isP("}") && curT() != TT::End) {
                e->args.push_back(parseVarInit());
                if (!eatP(",")) break;
            }
            expectP("}");
        }
        return e;
    }
    e->ty = parseTypeRef(false, false);
    if (isP("[")) {
        e->k = EK::NewArr;
        while (isP("[")) {
            adv();
            if (isP("]")) {
                adv();
                e->ty.dims++;
            } else {
                e->dims.push_back(parseExpr());
                expectP("]");
            }
        }
        if (isP("{")) {
            adv();
            while (!isP("}") && curT() != TT::End) {
                e->args.push_back(parseVarInit());
                if (!eatP(",")) break;
            }
            expectP("}");
        }
        return e;
    }
    if (e->ty.dims > 0 && isP("{")) {
        e->k = EK::NewArr;
        adv();
        while (!isP("}") && curT() != TT::End) {
            e->args.push_back(parseVarInit());
            if (!eatP(",")) break;
        }
        expectP("}");
        return e;
    }
    e->k = EK::New;
    expectP("(");
    while (!isP(")") && curT() != TT::End) {
        e->args.push_back(parseExpr());
        if (!eatP(",")) break;
    }
    expectP(")");
    if (isP("{")) {
        auto body = std::make_unique<AnonBody>();
        body->base = e->ty;
        body->line = line;
        Decl tmp;
        tmp.dk = 0;
        std::vector<std::pair<TypeRef, std::string>> none;
        parseTypeBody(tmp, false, false, &none);
        body->members = std::move(tmp.members);
        e->anon = std::move(body);
    }
    return e;
}
}
