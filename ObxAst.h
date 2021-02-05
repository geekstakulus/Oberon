#ifndef OBXAST_H
#define OBXAST_H

/*
* Copyright 2020 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the OBX parser/code model library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <Oberon/ObRowCol.h>
#include <bitset>
#include <QSharedData>
#include <QExplicitlySharedDataPointer>
#include <QVariant>
#include <QSet>

namespace Obx
{
// adopted from Ob::Ast

    struct AstVisitor;

    template <class T>
    struct Ref : public QExplicitlySharedDataPointer<T>
    {
        Ref(T* t = 0):QExplicitlySharedDataPointer<T>(t) {}
        bool isNull() const { return QExplicitlySharedDataPointer<T>::constData() == 0; }
    };

    template <class T>
    struct NoRef
    {
        T* d_ptr;
        NoRef(T* t = 0):d_ptr(t){}
        bool isNull() const { return d_ptr == 0; }
        T* data() const { return d_ptr; }
        T* operator->() const { return d_ptr; }
    };

    struct Thing : public QSharedData
    {
        enum Tag { T_Thing, T_Module, T_Import, T_Pointer, T_Record, T_BaseType, T_Array, T_ProcType, T_NamedType,
                   T_ArgExpr, T_Literal, T_SetExpr, T_IdentLeaf, T_UnExpr, T_IdentSel, T_BinExpr, T_Field,
                   T_Const, T_BuiltIn, T_Parameter, T_Return, T_Procedure, T_Variable, T_LocalVar,
                   T_QualiType, T_Call, T_Assign, T_IfLoop, T_ForLoop, T_CaseStmt, T_Scope,
                   T_Enumeration, T_GenericName, T_Exit,
                   T_MAX };
        static const char* s_tagName[];
        Ob::RowCol d_loc;
    #ifdef _DEBUG
        static QSet<Thing*> insts;
        Thing();
        virtual ~Thing();
    #else
        Thing() {}
        virtual ~Thing() {}
    #endif
        virtual bool isScope() const { return false; }
        virtual bool isNamed() const { return false; }
        virtual int getTag() const { return T_Thing; }
        virtual void accept(AstVisitor* v){}
        const char* getTagName() const { return s_tagName[getTag()]; }
    };

    template <typename T>
    inline T cast( Thing* in )
    {
    #ifdef _DEBUG
        T out = dynamic_cast<T>( in );
        Q_ASSERT( in == 0 || out != 0 );
    #else
        T out = static_cast<T>( in );
    #endif
        return out;
    }

    struct Type;
    struct BaseType;
    struct Pointer;
    struct Array;
    struct Record;
    struct ProcType;
    struct QualiType;
    struct Named;
    struct Field;
    struct Variable;
    struct LocalVar;
    struct Parameter;
    struct NamedType;
    struct Const;
    struct Import;
    struct BuiltIn;
    struct Scope;
    struct Procedure;
    struct Module;
    struct Statement;
    struct Call;
    struct Return;
    struct Assign;
    struct IfLoop;
    struct ForLoop;
    struct CaseStmt;
    struct Expression;
    struct Literal;
    struct SetExpr;
    struct IdentLeaf;
    struct UnExpr;
    struct IdentSel;
    struct ArgExpr;
    struct BinExpr;
    struct Enumeration;
    struct GenericName;
    struct Exit;

    typedef QList< Ref<Statement> > StatSeq;

    struct AstVisitor
    {
        virtual void visit( BaseType* ) {}
        virtual void visit( Pointer* ) {}
        virtual void visit( Array* ) {}
        virtual void visit( Record* ) {}
        virtual void visit( ProcType* ) {}
        virtual void visit( QualiType* ) {}
        virtual void visit( Field* ) {}
        virtual void visit( Variable* ) {}
        virtual void visit( LocalVar* ) {}
        virtual void visit( Parameter* ) {}
        virtual void visit( NamedType* ) {}
        virtual void visit( Const* ) {}
        virtual void visit( Import* ) {}
        virtual void visit( Procedure* ) {}
        virtual void visit( BuiltIn* ) {}
        virtual void visit( Module* ) {}
        virtual void visit( Call* ) {}
        virtual void visit( Return* ) {}
        virtual void visit( Assign* ) {}
        virtual void visit( IfLoop* ) {}
        virtual void visit( ForLoop* ) {}
        virtual void visit( CaseStmt* ) {}
        virtual void visit( Literal* ) {}
        virtual void visit( SetExpr* ) {}
        virtual void visit( IdentLeaf* ) {}
        virtual void visit( UnExpr* ) {}
        virtual void visit( IdentSel* ) {}
        virtual void visit( ArgExpr* ) {}
        virtual void visit( BinExpr* ) {}
        virtual void visit( Enumeration* ) {}
        virtual void visit( GenericName* ) {}
        virtual void visit( Exit* ) {}
    };

    struct Type : public Thing
    {
        Named* d_ident; // a reference to the ident or null if type is anonymous

        uint d_visited : 1;
        uint d_type : 4;    // used by BaseType
        uint d_selfRef : 1; // used by QualiType
        uint d_unsafe : 1;  // used by Pointer, Record (CSTRUCT, CUNION) and Array
        uint d_union : 1;   // used by Record (CUNION)

        Ref<Expression> d_flag; // optional system flag

        Type():d_ident(0),d_visited(false),d_type(0),d_selfRef(false),d_unsafe(false),d_union(false) {}
        typedef QList< Ref<Type> > List;
        virtual bool isStructured() const { return false; }
        virtual bool isSelfRef() const { return false; }
        virtual Type* derefed() { return this; }
        virtual QString pretty() const { return QString(); }
    };

    struct BaseType : public Type
    {
        enum { ANY, NIL, STRING, WSTRING, BOOLEAN, CHAR, WCHAR, BYTE, SHORTINT,
               INTEGER, LONGINT, REAL, LONGREAL, SET };
        static const char* s_typeName[];

        BaseType(quint8 t = NIL ) { d_type = t; }
        QVariant maxVal() const;
        QVariant minVal() const;
        int getTag() const { return T_BaseType; }
        void accept(AstVisitor* v) { v->visit(this); }
        const char* getTypeName() const { return s_typeName[d_type]; }
        QString pretty() const { return getTypeName(); }
    };

    struct Pointer : public Type
    {
        Ref<Type> d_to; // only to Record or Array
        int getTag() const { return T_Pointer; }
        void accept(AstVisitor* v) { v->visit(this); }
        QString pretty() const;
    };

    struct Array : public Type
    {
        quint32 d_len;  // zero for open arrays
        Ref<Expression> d_lenExpr;
        Ref<Type> d_type;
        Array():d_len(0) {}
        int getTag() const { return T_Array; }
        void accept(AstVisitor* v) { v->visit(this); }
        bool isStructured() const { return true; }
        Type* getTypeDim(int& dims , bool openOnly = false) const;
        QString pretty() const;
        QList<Array*> getDims();
    };

    struct Record : public Type
    {
        Ref<QualiType> d_base; // base type - a quali to a Record or Pointer or null
        Record* d_baseRec;
        QList<Record*> d_subRecs;
        Pointer* d_binding; // points back to pointer type in case of anonymous record

        typedef QHash<const char*,Named*> Names;
        Names d_names;
        QList< Ref<Field> > d_fields;
        QList< Ref<Procedure> > d_methods;

        Record():d_binding(0),d_baseRec(0) {}
        int getTag() const { return T_Record; }
        void accept(AstVisitor* v) { v->visit(this); }
        bool isStructured() const { return true; }
        Named* find(const QByteArray& name , bool recursive) const;
        QString pretty() const { return "RECORD"; }
    };

    struct ProcType : public Type
    {
        typedef QList< Ref<Parameter> > Formals;
        typedef QList<bool> Vars;

        Ref<Type> d_return; // QualiType actually
        Formals d_formals;

        ProcType(const Type::List& f, Type* r = 0);
        ProcType(const Type::List& f, const Vars& var, Type* r = 0);
        ProcType(){}
        int getTag() const { return T_ProcType; }
        Parameter* find( const QByteArray& ) const;
        void accept(AstVisitor* v) { v->visit(this); }
        bool isBuiltIn() const;
        QString pretty() const { return "PROC"; }
    };

    typedef QList< Ref<Thing> > MetaActuals;

    struct QualiType : public Type
    {
        typedef QPair<Named*,Named*> ModItem; // Module or 0 -> Item

        Ref<Expression> d_quali;
        MetaActuals d_metaActuals;

        QualiType(){}
        ModItem getQuali() const;
        bool isSelfRef() const { return d_selfRef; }
        int getTag() const { return T_QualiType; }
        void accept(AstVisitor* v) { v->visit(this); }
        Type* derefed();
        QString pretty() const;
    };

    struct Named : public Thing
    {
        enum Visibility { NotApplicable, Private, ReadWrite, ReadOnly };
        QByteArray d_name;
        Ref<Type> d_type;
        Scope* d_scope; // owning scope up to module (whose scope is nil)

        // Bytecode generator helpers
        uint d_liveFrom : 24; // 0..undefined
        uint d_slot : 8;
        uint d_liveTo : 24; // 0..undefined
        uint d_usedFromSubs : 1;
        uint d_usedFromLive : 1; // indirectly used named types
        uint d_initialized: 1;
        // end helpers

        uint d_slotValid : 1;
        uint d_visibility : 2; // Visibility
        uint d_synthetic: 1;
        uint d_hasErrors : 1;

        Named(const QByteArray& n = QByteArray(), Type* t = 0, Scope* s = 0):d_scope(s),d_type(t),d_name(n),
            d_visibility(NotApplicable),d_synthetic(false),d_liveFrom(0),d_liveTo(0),
            d_slot(0),d_slotValid(0),d_usedFromSubs(0),d_initialized(0),d_usedFromLive(0),
            d_hasErrors(0) {}
        bool isNamed() const { return true; }
        virtual bool isVarParam() const { return false; }
        Module* getModule();
        bool isPublic() const { return d_visibility == ReadWrite || d_visibility == ReadOnly; }
        const char* visibilitySymbol() const;
    };

    struct GenericName : public Named
    {
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_GenericName; }
    };
    typedef QList< Ref<GenericName> > MetaParams;

    struct Field : public Named // Record field
    {
        bool d_specialization; // field corresponds to inherited with same name, but more specific type
        Field():d_specialization(false){}
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_Field; }
    };

    struct Variable : public Named // Module variable
    {
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_Variable; }
    };

    struct LocalVar : public Named // Procedure local variable
    {
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_LocalVar; }
    };

    struct Parameter : public Named // Procedure parameter
    {
        bool d_var;
        bool d_const;
        bool d_receiver;
        Parameter():d_var(false),d_const(false),d_receiver(false) {}
        int getTag() const { return T_Parameter; }
        void accept(AstVisitor* v) { v->visit(this); }
        bool isVarParam() const { return ( d_var || d_const ); }
    };

    struct Const : public Named
    {
        QVariant d_val;
        quint8 d_vtype;
        Ref<Expression> d_constExpr;
        Const():d_vtype(0){}
        Const(const QByteArray& name, Literal* lit );
        int getTag() const { return T_Const; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct Enumeration : public Type
    {
        QList< Ref<Const> > d_items;
        int getTag() const { return T_Enumeration; }
        void accept(AstVisitor* v) { v->visit(this); }
        QString pretty() const { return "enumeration"; }
    };

    struct Import : public Named
    {
        QByteArrayList d_path;
        Ob::RowCol d_aliasPos; // invalid if no alias present
        Ref<Module> d_mod;
        int getTag() const { return T_Import; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct BuiltIn : public Named
    {
        enum { // Oberon-07
               ABS, ODD, LEN, LSL, ASR, ROR, FLOOR, FLT, ORD, CHR, INC, DEC, INCL, EXCL,
               NEW, ASSERT, PACK, UNPK,
               LED, // LED not global proc in Oberon report, but used as such in Project Oberon
               // IDE
               TRAP, TRAPIF,
               // SYSTEM
               SYS_ADR, SYS_BIT, SYS_GET, SYS_H, SYS_LDREG, SYS_PUT, SYS_REG, SYS_VAL, SYS_COPY,
               // Oberon-2
               MAX, CAP, LONG, SHORT, HALT, COPY, ASH, MIN, SIZE, ENTIER,
               // Blackbox
               BITS,
               // Oberon-2 SYSTEM
               SYS_MOVE, SYS_NEW, SYS_ROT, SYS_LSH, SYS_GETREG, SYS_PUTREG,
               // Blackbox SYSTEM
               SYS_TYP,
               // Oberon+
               VAL, STRLEN, WCHR
             };
        static const char* s_typeName[];
        quint8 d_func;
        BuiltIn(quint8 f, ProcType* = 0 );
        int getTag() const { return T_BuiltIn; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct Scope : public Named
    {
        typedef QHash<const char*, Named*> Names;
        Names d_names;
        QList< Ref<Named> > d_order;
        QList< Ref<IdentLeaf> > d_helper; // filled with helper decls when fillXref
        StatSeq d_body;
        Ob::RowCol d_end;

        bool isScope() const { return true; }
        int getTag() const { return T_Scope; }

        Named* find( const QByteArray&, bool recursive = true ) const;
        bool add( Named* );
    };

    struct Procedure : public Scope
    {
        Ref<Parameter> d_receiver;
        Record* d_receiverRec; // the record to which this procedure is bound
        Procedure* d_super; // the procedure of the super class this procedure overrides, or zero
        QList<Procedure*> d_subs; // the procedures of the subclasses which override this procedure
        MetaParams d_metaParams;
        Ref<Expression> d_imp; // the number or string after PROC+
        Procedure():d_receiverRec(0),d_super(0) {}
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_Procedure; }
        ProcType* getProcType() const;
    };

    struct Module : public Scope
    {
        QList<Import*> d_imports;
        QString d_file;
        QByteArrayList d_fullName; // Path segments (if present) + module name
        bool d_isValidated;
        bool d_isDef; // DEFINITION module
        bool d_isExt;
        QList< Ref<Type> > d_helper2; // filled with pointers because of ADDROF

        Module():d_isDef(false),d_isValidated(false),d_isExt(false) {}
        int getTag() const { return T_Module; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct NamedType : public Scope // TypeDeclaration (Scope because of MetaParams)
    {
        MetaParams d_metaParams;

        NamedType( const QByteArray& n, Type* t ) { d_name = n; d_type = t; }
        NamedType() {}
        int getTag() const { return T_NamedType; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct Statement : public Thing
    {
    };

    struct Call : public Statement
    {
        Ref<Expression> d_what;
        void accept(AstVisitor* v) { v->visit(this); }
        ArgExpr* getCallExpr() const;
        int getTag() const { return T_Call; }
    };

    struct Return : public Statement
    {
        Ref<Expression> d_what;
        int getTag() const { return T_Return; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct Exit : public Statement
    {
        int getTag() const { return T_Exit; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct Assign : public Statement
    {
        Ref<Expression> d_lhs;
        Ref<Expression> d_rhs;
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_Assign; }
    };

    typedef QList< Ref<Expression> > ExpList;

    struct IfLoop : public Statement
    {
        enum { IF, WHILE, REPEAT, WITH, LOOP };
        quint8 d_op;
        ExpList d_if;
        QList<StatSeq> d_then;
        StatSeq d_else;
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_IfLoop; }
    };

    struct ForLoop : public Statement
    {
        Ref<Expression> d_id, d_from, d_to, d_by;
        QVariant d_byVal;
        StatSeq d_do;
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_ForLoop; }
    };

    struct CaseStmt : public Statement
    {
        Ref<Expression> d_exp;
        struct Case
        {
            ExpList d_labels;
            StatSeq d_block;
        };
        QList<Case> d_cases;
        StatSeq d_else;
        bool d_typeCase;
        CaseStmt():d_typeCase(false){}
        void accept(AstVisitor* v) { v->visit(this); }
        int getTag() const { return T_CaseStmt; }
    };

    enum IdentRole { NoRole, DeclRole, LhsRole, VarRole, RhsRole, SuperRole, SubRole, CallRole,
                     ImportRole, ThisRole, MethRole, StringRole };

    struct Expression : public Thing
    {
        NoRef<Type> d_type; // this must be NoRef, otherwise there are refcount cycles!
        virtual Named* getIdent() const { return 0; }
        virtual Module* getModule() const { return 0; }
        virtual quint8 visibilityFor(Module*) const { return Named::NotApplicable; }
        virtual Expression* getSub() const { return 0; }
        QList<Expression*> getSubList() const;
        virtual quint8 getUnOp() const { return 0; }
        virtual IdentRole getIdentRole() const { return NoRole; }
    };

    struct Literal : public Expression
    {
        enum { SET_BIT_LEN = 32 };
        typedef std::bitset<SET_BIT_LEN> SET;

        enum ValueType { Invalid, Integer, Real, Boolean, String /* QBA utf8 */, Bytes /* QBA */, Char /* quint16 */, Nil, Set };
        QVariant d_val;
        uint d_vtype : 8;
        uint d_strLen : 24;
        Literal( ValueType t = Invalid, Ob::RowCol l = Ob::RowCol(),
                 const QVariant& v = QVariant(), Type* typ = 0 ):d_val(v),d_vtype(t),d_strLen(0){ d_loc = l; d_type = typ; }
        int getTag() const { return T_Literal; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct SetExpr : public Expression
    {
        ExpList d_parts;
        int getTag() const { return T_SetExpr; }
        void accept(AstVisitor* v) { v->visit(this); }
    };

    struct IdentLeaf : public Expression
    {
        NoRef<Named> d_ident;
        QByteArray d_name; // name to be resolved with result written to d_ident
        IdentRole d_role;
        IdentLeaf():d_mod(0),d_role(NoRole) {}
        IdentLeaf( Named* id, const Ob::RowCol&, Module* mod, Type* t, IdentRole r );
        Module* d_mod; // we need this to find out when xref from which module the ident is coming
        Named* getIdent() const { return d_ident.data(); }
        Module* getModule() const { return d_mod; }
        int getTag() const { return T_IdentLeaf; }
        void accept(AstVisitor* v) { v->visit(this); }
        quint8 visibilityFor(Module*) const { return Named::ReadWrite; } // leaf is local or import name
        IdentRole getIdentRole() const { return d_role; }
    };

    struct UnExpr : public Expression
    {
        enum Op { Invalid, NEG, NOT, DEREF, ADDROF, // implemented in UnExpr
                  CAST, SEL, CALL, IDX // implemented in subclasses
                };
        static const char* s_opName[];
        quint8 d_op;
        Ref<Expression> d_sub;
        UnExpr(quint8 op = Invalid, Expression* e = 0 ):d_op(op),d_sub(e){}
        int getTag() const { return T_UnExpr; }
        void accept(AstVisitor* v) { v->visit(this); }
        quint8 visibilityFor(Module* m) const { return !d_sub.isNull() ? d_sub->visibilityFor(m): quint8(Named::NotApplicable); }
        Module* getModule() const { return d_sub.isNull() ? 0 : d_sub->getModule(); }
        Expression* getSub() const { return d_sub.data(); }
        quint8 getUnOp() const { return d_op; }
    };

    struct IdentSel : public UnExpr // SEL
    {
        NoRef<Named> d_ident;
        QByteArray d_name; // name to be resolved with result written to d_ident
        IdentRole d_role;
        IdentSel():UnExpr(SEL),d_role(NoRole) {}
        Named* getIdent() const { return d_ident.data(); }
        int getTag() const { return T_IdentSel; }
        void accept(AstVisitor* v) { v->visit(this); }
        quint8 visibilityFor(Module* m) const;
        IdentRole getIdentRole() const { return d_role; }
    };

    struct ArgExpr : public UnExpr // CALL, IDX, or CAST
    {
        ExpList d_args;
        int getTag() const { return T_ArgExpr; }
        void accept(AstVisitor* v) { v->visit(this); }
        ProcType* getProcType() const;
    };

    struct BinExpr : public Expression
    {
        enum Op { Invalid, Range,
                  // relations:
                  EQ, NEQ, LT, LEQ, GT, GEQ, IN, IS,  //'=' | '#' | '<' | '<=' | '>' | '>=' | IN | IS
                  // AddOperator
                  ADD, SUB, OR,  // '+' | '-' | OR
                  // MulOperator
                  MUL, FDIV, DIV, MOD, AND,  // '*' | '/' | DIV | MOD | '&'
                };
        static const char* s_opName[];
        quint8 d_op;
        Ref<Expression> d_lhs, d_rhs;
        BinExpr():d_op(Invalid){}
        int getTag() const { return T_BinExpr; }
        void accept(AstVisitor* v) { v->visit(this); }
        Module* getModule() const { return !d_lhs.isNull() ? d_lhs->getModule() : !d_rhs.isNull() ? d_rhs->getModule() : 0 ; }
    };

}

Q_DECLARE_METATYPE( Obx::Literal::SET )

#endif // OBXAST_H
