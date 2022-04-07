// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include "lang.h"

#include "wf.h"

namespace sample
{
  PassDef modules()
  {
    return {
      // Module.
      T(Directory)[Directory] << (T(File)++)[File] >>
        [](auto& _) {
          auto ident = path::last(_(Directory)->location().source->origin());
          return Group << (Class ^ _(Directory)) << (Ident ^ ident)
                       << (Brace << *_[File]);
        },

      // File on its own (no module). This rewrites to a class to prevent it
      // from being placed in a symbol table in the next pass.
      In(Group) * T(File)[File] >>
        [](auto& _) {
          auto ident = path::last(_(File)->location().source->origin());
          return (Class ^ ident)
            << Typeparams << Type << (Classbody << *_[File]);
        },
    };
  }

  PassDef types()
  {
    return {
      // Packages.
      T(Package) * (T(String) / T(Escaped))[String] >>
        [](auto& _) { return Package << _[String]; },

      // Type.
      T(Colon)[Colon] * ((!T(Brace))++)[Type] >>
        [](auto& _) { return (Type ^ _(Colon)) << _[Type]; },
    };
  }

  inline const auto ExprStruct =
    In(Funcbody) / In(Assign) / In(Tuple) / In(Expr) / In(Term);
  inline const auto TermStruct = In(Term) / In(Expr);
  inline const auto TypeStruct = In(Type) / In(TypeTerm) / In(TypeTuple);
  inline const auto Name = T(Ident) / T(Symbol);
  inline const auto Literal = T(String) / T(Escaped) / T(Char) / T(Bool) /
    T(Hex) / T(Bin) / T(Int) / T(Float) / T(HexFloat);

  PassDef structure()
  {
    return {
      // Field: (group let ident type)
      In(Classbody) * T(Group)
          << (T(Let) * T(Ident)[id] * ~T(Type)[Type] * End) >>
        [](auto& _) { return _(id = FieldLet) << (_[Type] | Type) << Expr; },

      // Field: (group var ident type)
      In(Classbody) * T(Group)
          << (T(Var) * T(Ident)[id] * ~T(Type)[Type] * End) >>
        [](auto& _) { return _(id = FieldVar) << (_[Type] | Type) << Expr; },

      // Field: (equals (group var ident type) group)
      In(Classbody) * T(Equals)
          << ((T(Group) << (T(Var) * T(Ident)[id] * ~T(Type)[Type] * End)) *
              T(Group)[rhs] * End) >>
        [](auto& _) {
          return _(id = FieldVar) << (_[Type] | Type) << (Expr << *_[rhs]);
        },

      // Field: (equals (group let ident type) group)
      In(Classbody) * T(Equals)
          << ((T(Group) << (T(Let) * T(Ident)[id] * ~T(Type)[Type] * End)) *
              T(Group)[rhs] * End) >>
        [](auto& _) {
          return _(id = FieldLet) << (_[Type] | Type) << (Expr << *_[rhs]);
        },

      // Function.
      In(Classbody) * T(Group)
          << (~Name[id] * ~T(Square)[Typeparams] * T(Paren)[Params] *
              ~T(Type)[Type] * ~T(Brace)[Funcbody] * (Any++)[rhs]) >>
        [](auto& _) {
          _.def(id, apply);
          return Seq << (_(id = Function)
                         << (Typeparams << *_[Typeparams])
                         << (Params << *_[Params]) << (_[Type] | Type)
                         << (Funcbody << *_[Funcbody]))
                     << (Group << _[rhs]);
        },

      // Typeparams.
      T(Typeparams) << T(List)[Typeparams] >>
        [](auto& _) { return Typeparams << *_[Typeparams]; },

      // Typeparam: (group ident type)
      In(Typeparams) * T(Group) << (T(Ident)[id] * ~T(Type)[Type] * End) >>
        [](auto& _) { return _(id = Typeparam) << (_[Type] | Type) << Type; },

      // Typeparam: (equals (group ident type) group)
      In(Typeparams) * T(Equals)
          << ((T(Group) << (T(Ident)[id] * ~T(Type)[Type] * End)) *
              T(Group)[rhs] * End) >>
        [](auto& _) {
          return _(id = Typeparam) << (_[Type] | Type) << (Type << *_[rhs]);
        },

      // Params.
      T(Params) << T(List)[Params] >>
        [](auto& _) { return Params << *_[Params]; },

      // Param: (group ident type)
      In(Params) * T(Group) << (T(Ident)[id] * ~T(Type)[Type] * End) >>
        [](auto& _) { return _(id = Param) << (_[Type] | Type) << Expr; },

      // Param: (equals (group ident type) group)
      In(Params) * T(Equals)
          << ((T(Group) << (T(Ident)[id] * ~T(Type)[Type] * End)) *
              T(Group)[Expr] * End) >>
        [](auto& _) {
          return _(id = Param) << (_[Type] | Type) << (Expr << *_[Expr]);
        },

      // Use.
      (In(Classbody) / In(Funcbody)) * T(Group)
          << T(Use)[Use] * (Any++)[Type] >>
        [](auto& _) { return (Use ^ _(Use)) << (Type << _[Type]); },

      // Typealias.
      (In(Classbody) / In(Funcbody)) * T(Group)
          << (T(Typealias) * T(Ident)[id] * ~T(Square)[Typeparams] *
              ~T(Type)[Type] * End) >>
        [](auto& _) {
          return _(id = Typealias)
            << (Typeparams << *_[Typeparams]) << (_[Type] | Type) << Type;
        },

      // Typealias: (equals (typealias typeparams type type) group)
      (In(Classbody) / In(Funcbody)) * T(Equals)
          << ((T(Group)
               << (T(Typealias) * T(Ident)[id] * ~T(Square)[Typeparams] *
                   ~T(Type)[Type] * End)) *
              T(Group)[rhs] * End) >>
        [](auto& _) {
          return _(id = Typealias) << (Typeparams << *_[Typeparams])
                                   << (_[Type] | Type) << (Type << *_[rhs]);
        },

      // Class.
      (In(Classbody) / In(Funcbody)) * T(Group)
          << (T(Class) * T(Ident)[id] * ~T(Square)[Typeparams] *
              ~T(Type)[Type] * T(Brace)[Classbody] * (Any++)[rhs]) >>
        [](auto& _) {
          return Seq << (_(id = Class)
                         << (Typeparams << *_[Typeparams]) << (_[Type] | Type)
                         << (Classbody << *_[Classbody]))
                     << (Group << _[rhs]);
        },

      // Type structure.
      TypeStruct * T(Group)[TypeTerm] >>
        [](auto& _) { return TypeTerm << *_[TypeTerm]; },
      TypeStruct * T(List)[TypeTuple] >>
        [](auto& _) { return TypeTuple << *_[TypeTuple]; },
      TypeStruct * T(Paren)[TypeTerm] >>
        [](auto& _) { return TypeTerm << *_[TypeTerm]; },

      // Interfaces.
      TypeStruct * T(Brace)[Classbody] >>
        [](auto& _) { return TypeTrait << (Classbody << *_[Classbody]); },

      // Expression structure.
      ExprStruct * T(Group)[Expr] >> [](auto& _) { return Expr << *_[Expr]; },
      ExprStruct * T(List)[Tuple] >> [](auto& _) { return Tuple << *_[Tuple]; },
      ExprStruct * T(Equals)[Assign] >>
        [](auto& _) { return Assign << *_[Assign]; },
      ExprStruct * T(Paren)[Term] >> [](auto& _) { return Term << *_[Term]; },

      // Term compaction.
      T(Term) << ((T(Term) / T(Tuple) / T(Assign))[op] * End) >>
        [](auto& _) { return _(op); },
      T(Term) << (T(Expr)[Expr] * End) >>
        [](auto& _) { return Term << *_[Expr]; },
      T(Expr) << (T(Term)[Term] * End) >>
        [](auto& _) { return Expr << *_[Term]; },
      T(Term) << End >> [](auto& _) -> Node { return Tuple; },

      (In(Type) / ExprStruct) * T(Square)[Typeargs] >>
        [](auto& _) { return Typeargs << *_[Typeargs]; },
      T(Typeargs) << T(List)[Typeargs] >>
        [](auto& _) { return Typeargs << *_[Typeargs]; },
      In(Typeargs) * T(Group)[Type] >> [](auto& _) { return Type << *_[Type]; },
      In(Typeargs) * T(Paren)[Type] >> [](auto& _) { return Type << *_[Type]; },

      // Lambda: (group typeparams) (list params...) => rhs
      ExprStruct * T(Brace)
          << (((T(Group) << T(Square)[Typeparams]) * T(List)[Params]) *
              (T(Group) << T(FatArrow)) * (Any++)[rhs]) >>
        [](auto& _) {
          return Lambda << (Typeparams << *_[Typeparams])
                        << (Params << *_[Params]) << (Funcbody << _[rhs]);
        },

      // Lambda: (group typeparams) (group param) => rhs
      ExprStruct * T(Brace)
          << (((T(Group) << T(Square)[Typeparams]) * T(Group)[Param]) *
              (T(Group) << T(FatArrow)) * (Any++)[rhs]) >>
        [](auto& _) {
          return Lambda << (Typeparams << *_[Typeparams])
                        << (Params << _[Param]) << (Funcbody << _[rhs]);
        },

      // Lambda: (list (group typeparams? param) params...) => rhs
      ExprStruct * T(Brace)
          << ((T(List)
               << ((T(Group) << (~T(Square)[Typeparams] * (Any++)[Param])) *
                   (Any++)[Params]))) *
            (T(Group) << T(FatArrow)) * (Any++)[rhs] >>
        [](auto& _) {
          return Lambda << (Typeparams << *_[Typeparams])
                        << (Params << (Group << _[Param]) << _[Params])
                        << (Funcbody << _[rhs]);
        },

      // Lambda: (group typeparams? param) => rhs
      ExprStruct * T(Brace)
          << ((T(Group) << (~T(Square)[Typeparams] * (Any++)[Param])) *
              (T(Group) << T(FatArrow)) * (Any++)[rhs]) >>
        [](auto& _) {
          return Lambda << (Typeparams << *_[Typeparams])
                        << (Params << (Group << _[Param]) << _[Params])
                        << (Funcbody << _[rhs]);
        },

      // Zero argument lambda.
      ExprStruct * T(Brace) << (!(T(Group) << T(FatArrow)))++[Lambda] >>
        [](auto& _) {
          return Lambda << Typeparams << Params << (Funcbody << _[Lambda]);
        },

      // Var.
      ExprStruct * Start * T(Var) * T(Ident)[id] >>
        [](auto& _) { return _(id = Var); },

      // Let.
      ExprStruct * Start * T(Let) * T(Ident)[id] >>
        [](auto& _) { return _(id = Let); },

      // Throw.
      ExprStruct * Start * T(Throw) * (Any++)[rhs] >>
        [](auto& _) { return Throw << (Expr << _[rhs]); },

      // Empty groups.
      T(Group) << End >> [](auto& _) -> Node { return {}; },
    };
  }

  Token reftype(Node def)
  {
    static std::map<Token, Token> map{
      {Var, RefVar},
      {Let, RefLet},
      {Param, RefParam},
      {Class, RefClass},
      {Typealias, RefTypealias},
      {Typeparam, RefTypeparam},
      {Function, RefFunction},
    };

    if (!def)
      return Selector;

    auto it = map.find(def->type());
    if (it == map.end())
      return Selector;

    return it->second;
  }

  PassDef reftype()
  {
    return {
      dir::bottomup,
      {
        TypeStruct * (T(Ident)[id] * ~T(Typeargs)[Typeargs])[Type] >>
          [](auto& _) {
            auto def = look->at(_[Type]).def;
            return reftype(def) << _[id] << (_[Typeargs] | Typeargs);
          },

        TypeStruct *
            ((T(RefClass) / T(RefTypealias) / T(RefTypeparam) /
              T(Package))[lhs] *
             T(DoubleColon) * T(Ident)[id] * ~T(Typeargs)[Typeargs])[Type] >>
          [](auto& _) {
            auto def = look->at(_[Type]).def;
            return reftype(def) << _[lhs] << _[id] << (_[Typeargs] | Typeargs);
          },
      }};
  }

  inline const auto TypeElem = T(TypeTerm) / T(RefClass) / T(RefTypealias) /
    T(RefTypeparam) / T(TypeTuple) / T(Iso) / T(Imm) / T(Mut) / T(TypeView) /
    T(TypeFunc) / T(TypeThrow) / T(TypeIsect) / T(TypeUnion) / T(TypeVar) /
    T(TypeTrait) / T(DontCare);

  PassDef typeexpr()
  {
    return {
      TypeStruct * TypeElem[lhs] * T(Symbol, "~>") * TypeElem[rhs] *
          --T(Symbol, "~>") >>
        [](auto& _) { return TypeView << _[lhs] << _[rhs]; },
      TypeStruct * TypeElem[lhs] * T(Symbol, "->") * TypeElem[rhs] *
          --T(Symbol, "->") >>
        [](auto& _) { return TypeFunc << _[lhs] << _[rhs]; },
    };
  }

  PassDef typealg()
  {
    return {
      TypeStruct * TypeElem[lhs] * T(Symbol, "&") * TypeElem[rhs] >>
        [](auto& _) { return TypeIsect << _[lhs] << _[rhs]; },
      TypeStruct * TypeElem[lhs] * T(Symbol, "\\|") * TypeElem[rhs] >>
        [](auto& _) { return TypeUnion << _[lhs] << _[rhs]; },
      TypeStruct * T(Throw) * TypeElem[rhs] >>
        [](auto& _) { return TypeThrow << _[rhs]; },
      T(TypeTerm) << (TypeElem[op] * End) >> [](auto& _) { return _(op); },
    };
  }

  PassDef dnf()
  {
    return {
      T(TypeIsect)
          << (((!T(TypeUnion))++)[lhs] * T(TypeUnion)[op] * (Any++)[rhs]) >>
        [](auto& _) {
          Node r = TypeUnion;
          for (auto& child : *_(op))
            r << (TypeIsect << clone(child) << clone(_[lhs]) << clone(_[rhs]));
          return r;
        },
    };
  }

  PassDef refexpr()
  {
    return {
      dir::bottomup,
      {
        // Flatten algebraic types.
        In(TypeUnion) * T(TypeUnion)[lhs] >>
          [](auto& _) { return Seq << *_[lhs]; },
        In(TypeIsect) * T(TypeIsect)[lhs] >>
          [](auto& _) { return Seq << *_[lhs]; },

        // Identifiers and symbols.
        TermStruct * T(Dot) * Name[id] * ~T(Typeargs)[Typeargs] >>
          [](auto& _) {
            return DotSelector << _[id] << (_[Typeargs] | Typeargs);
          },

        TermStruct * (Name[id] * ~T(Typeargs)[Typeargs])[Type] >>
          [](auto& _) {
            auto def = look->at(_[Type]).def;
            return reftype(def) << _[id] << (_[Typeargs] | Typeargs);
          },

        // Scoped lookup.
        TermStruct *
            ((T(RefClass) / T(RefTypealias) / T(RefTypeparam) /
              T(Package))[lhs] *
             T(DoubleColon) * Name[id] * ~T(Typeargs)[Typeargs])[Type] >>
          [](auto& _) {
            auto def = look->at(_[Type]).def;
            return reftype(def) << _[lhs] << _[id] << (_[Typeargs] | Typeargs);
          },

        // Use.
        T(Use)[lhs]
            << (T(Type)
                << (T(RefClass) / T(RefTypealias) / T(RefTypeparam) /
                    T(Package))[rhs]) >>
          [](auto& _) {
            auto site = Include ^ _(lhs);
            _.include(site, look->at(_[rhs]).def);
            return site << _[rhs];
          },

        // Create sugar.
        TermStruct * (T(RefClass) / T(RefTypeparam))[lhs] >>
          [](auto& _) {
            return Call << (RefFunction << _[lhs] << (Ident ^ create)
                                        << Typeargs)
                        << Tuple;
          },
      }};
  }

  inline const auto Object = Literal / T(RefVar) / T(RefLet) / T(RefParam) /
    T(Tuple) / T(Lambda) / T(Call) / T(Term) / T(Assign) / T(DontCare);
  inline const auto Operator = T(RefFunction) / T(Selector);

  PassDef typeassert()
  {
    return {
      // Type assertions for operators.
      T(Term) << (Operator[op] * T(Type)[Type] * End) >>
        [](auto& _) { return _(op) << _[Type]; },
    };
  }

  PassDef reverseapp()
  {
    return {
      // Dot: reverse application.
      TermStruct * Object[lhs] * T(Dot) * Any[rhs] >>
        [](auto& _) { return Call << _[rhs] << _[lhs]; },

      TermStruct * Object[lhs] * T(DotSelector)[rhs] >>
        [](auto& _) { return Call << (Selector << *_[rhs]) << _[lhs]; },
    };
  }

  PassDef application()
  {
    return {
      // Adjacency: application.
      TermStruct * Object[lhs] * Object[rhs] >>
        [](auto& _) { return Call << _[lhs] << _[rhs]; },

      // Prefix.
      TermStruct * Operator[op] * Object[rhs] >>
        [](auto& _) { return Call << _[op] << _[rhs]; },

      // Infix.
      TermStruct * T(Tuple)[lhs] * Operator[op] * T(Tuple)[rhs] >>
        [](auto& _) { return Call << _[op] << (Tuple << *_[lhs] << *_[rhs]); },
      TermStruct * T(Tuple)[lhs] * Operator[op] * Object[rhs] >>
        [](auto& _) { return Call << _[op] << (Tuple << *_[lhs] << _[rhs]); },
      TermStruct * Object[lhs] * Operator[op] * T(Tuple)[rhs] >>
        [](auto& _) { return Call << _[op] << (Tuple << _[lhs] << *_[rhs]); },
      TermStruct * Object[lhs] * Operator[op] * Object[rhs] >>
        [](auto& _) { return Call << _[op] << (Tuple << _[lhs] << _[rhs]); },

      // Term compaction.
      T(Term) << (Any[op] * End) >> [](auto& _) { return _(op); },
    };
  }

  inline const auto InContainer =
    In(Expr) / In(Term) / In(Tuple) / In(Assign) / In(Call);
  inline const auto Container =
    T(Expr) / T(Term) / T(Tuple) / T(Assign) / T(Call) / T(Lift);
  inline const auto LiftExpr = T(Term) / T(Lambda) / T(Call) / T(Assign);

  PassDef anf()
  {
    return {
      InContainer * LiftExpr[Lift] >>
        [](auto& _) {
          auto e = _(Lift);
          auto id = e->fresh();
          return Lift << _(id = Let) << e
                      << (RefLet << (Ident ^ id) << Typeargs);
        },

      Container[op]
          << (((!T(Lift))++)[lhs] *
              (T(Lift) << ((T(Let) * Any)[Let] * ~T(RefLet)[RefLet] * End)) *
              (Any++)[rhs]) >>
        [](auto& _) {
          // Pull the lift out of the container, leaving the reflet behind.
          return Seq << (Lift << _[Let])
                     << (_(op)->type() << _[lhs] << _[RefLet] << _[rhs]);
        },
    };
  }

  Driver& driver()
  {
    static Driver d(
      "Verona",
      parser(),
      {
        {"modules", modules()},
        {"types", types()},
        {"structure", structure()},
        {"reftype", reftype()},
        {"typeexpr", typeexpr()},
        {"typealg", typealg()},
        {"dnf", dnf()},
        {"refexpr", refexpr()},
        {"typeassert", typeassert()},
        {"reverseapp", reverseapp()},
        {"application", application()},
        {"anf", anf()},
        {"infer", infer()},
      });

    return d;
  }
}
