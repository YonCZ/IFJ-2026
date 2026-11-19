# LL tabulka

Skript `ll_check.py` ověřil tabulku bez kolizí a 303 testů. Prázdná buňka znamená syntaktickou chybu 2.

| Neterminál | Tokeny | Pravidlo |
|---|---|---|
| `AfterCommaParam` | `ID` | `Param ParamRest` |
| `AfterCommaParam` | `RPAREN` | `ε` |
| `Block` | `DOUBLE_NUM`, `FALSE_TOKEN`, `FOR`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `BlockItem` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR` | `SimpleStmt LineEnd` |
| `BlockItem` | `IF` | `IfStmt` |
| `BlockItem` | `WHILE` | `WhileStmt` |
| `BlockItem` | `FOR` | `ForStmt` |
| `Body` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `Rhs EOL` |
| `Body` | `EOL` | `EOL INDENT Block DEDENT` |
| `CaseClassDef` | `CASE` | `CASE CLASS ID LPAREN ParamList RPAREN EOL` |
| `DoBody` | `EOL` | `EOL INDENT Block DEDENT` |
| `DoBody` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `Rhs LineEnd` |
| `ExtDefs` | `EOL` | `EOL ExtDefs` |
| `ExtDefs` | `AT`, `DEF` | `FuncDef ExtDefs` |
| `ExtDefs` | `DEDENT` | `ε` |
| `ExtensionDef` | `EXTENSION` | `EXTENSION LPAREN Param RPAREN EOL INDENT ExtDefs DEDENT` |
| `ForGen` | `ID` | `ID LARROW Expr EOL` |
| `ForGenInline` | `ID` | `ID LARROW Expr` |
| `ForGuardRest` | `EOL` | `EOL ForGuardRest` |
| `ForGuardRest` | `IF` | `IF Expr EOL ForGuardRest` |
| `ForGuardRest` | `DEDENT` | `ε` |
| `ForHead` | `EOL` | `EOL INDENT ForGen ForGuardRest DEDENT DO DoBody` |
| `ForHead` | `ID` | `ForGenInline DO DoBody` |
| `ForStmt` | `FOR` | `FOR ForHead` |
| `FuncDef` | `DEF` | `DEF ID Params RetType ASSIGN Body` |
| `FuncDef` | `AT` | `AT ID OptEol DEF ID Params RetType ASSIGN Body` |
| `FuncDefs` | `EOL` | `EOL FuncDefs` |
| `FuncDefs` | `AT`, `DEF` | `FuncDef FuncDefs` |
| `FuncDefs` | `EXTENSION` | `ExtensionDef FuncDefs` |
| `FuncDefs` | `TYPE` | `TypeDef FuncDefs` |
| `FuncDefs` | `CASE` | `CaseClassDef FuncDefs` |
| `FuncDefs` | `EOF` | `ε` |
| `IfElse` | `EOL` | `EOL INDENT Block DEDENT` |
| `IfElse` | `IF` | `IfStmt` |
| `IfElse` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `ExprNoIf LineEnd` |
| `IfElseOpt` | `ELSE` | `ELSE IfElse` |
| `IfElseOpt` | `EOL`, `SEMICOLON` | `LineEnd` |
| `IfStmt` | `IF` | `IF Expr THEN IfThen` |
| `IfTail` | `ELSE` | `ELSE IfElse` |
| `IfTail` | `DEDENT`, `DOUBLE_NUM`, `EOL`, `FALSE_TOKEN`, `FOR`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `ε` |
| `IfThen` | `EOL` | `EOL INDENT Block DEDENT IfTail` |
| `IfThen` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `Rhs IfElseOpt` |
| `InterpContent` | `LBRACE` | `LBRACE Expr RBRACE InterpContent` |
| `InterpContent` | `ID` | `ID InterpContent` |
| `InterpContent` | `INTERP_MID` | `INTERP_MID InterpContent` |
| `InterpContent` | `INTERP_END` | `INTERP_END` |
| `InterpString` | `INTERP_START` | `INTERP_START InterpContent` |
| `LineEnd` | `EOL` | `EOL` |
| `LineEnd` | `SEMICOLON` | `SEMICOLON MidItem` |
| `MidItem` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR` | `MidSimple LineEnd` |
| `MidItem` | `IF` | `IfStmt` |
| `MidItem` | `WHILE` | `WhileStmt` |
| `MidItem` | `FOR` | `ForStmt` |
| `MidItem` | `EOL` | `EOL` |
| `MidSimple` | `VAL` | `ValStmt` |
| `MidSimple` | `VAR` | `VarStmt` |
| `MidSimple` | `RETURN` | `ReturnStmt` |
| `MidSimple` | `ID` | `ID StmtIdTail` |
| `MidSimple` | `DOUBLE_NUM`, `FALSE_TOKEN`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `ExprNoId` |
| `OptEol` | `EOL` | `EOL` |
| `OptEol` | `DEF` | `ε` |
| `Param` | `ID` | `ID COLON Type` |
| `ParamGroup` | `LPAREN` | `LPAREN ParamList RPAREN` |
| `ParamList` | `ID` | `Param ParamRest` |
| `ParamList` | `RPAREN` | `ε` |
| `ParamRest` | `COMMA` | `COMMA AfterCommaParam` |
| `ParamRest` | `RPAREN` | `ε` |
| `Params` | `LPAREN` | `ParamGroup Params` |
| `Params` | `ASSIGN`, `COLON` | `ε` |
| `PatArg` | `ID` | `ID` |
| `PatArg` | `INT_NUM` | `INT_NUM` |
| `PatArg` | `DOUBLE_NUM` | `DOUBLE_NUM` |
| `PatArg` | `STRING_LITERAL` | `STRING_LITERAL` |
| `PatArg` | `TRUE_TOKEN` | `TRUE_TOKEN` |
| `PatArg` | `FALSE_TOKEN` | `FALSE_TOKEN` |
| `PatArg` | `UNDERSCORE` | `UNDERSCORE` |
| `PatArg` | `MINUS` | `MINUS PatNeg` |
| `PatList` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INT_NUM`, `MINUS`, `STRING_LITERAL`, `TRUE_TOKEN`, `UNDERSCORE` | `PatArg PatRest` |
| `PatList` | `RPAREN` | `ε` |
| `PatNeg` | `INT_NUM` | `INT_NUM` |
| `PatNeg` | `DOUBLE_NUM` | `DOUBLE_NUM` |
| `PatRest` | `COMMA` | `COMMA PatArg PatRest` |
| `PatRest` | `RPAREN` | `ε` |
| `PatTail` | `COLON` | `COLON TypeAtom` |
| `PatTail` | `LPAREN` | `LPAREN PatList RPAREN` |
| `Pattern` | `INT_NUM` | `INT_NUM` |
| `Pattern` | `DOUBLE_NUM` | `DOUBLE_NUM` |
| `Pattern` | `STRING_LITERAL` | `STRING_LITERAL` |
| `Pattern` | `TRUE_TOKEN` | `TRUE_TOKEN` |
| `Pattern` | `FALSE_TOKEN` | `FALSE_TOKEN` |
| `Pattern` | `UNDERSCORE` | `UNDERSCORE` |
| `Pattern` | `LPAREN` | `LPAREN RPAREN` |
| `Pattern` | `MINUS` | `MINUS PatNeg` |
| `Pattern` | `ID` | `ID PatTail` |
| `Program` | `IMPORT` | `Prolog FuncDefs EOF` |
| `Prolog` | `IMPORT` | `IMPORT ID DOT MUL EOL` |
| `RetType` | `COLON` | `COLON Type` |
| `RetType` | `ASSIGN` | `ε` |
| `ReturnStmt` | `RETURN` | `RETURN RhsOpt` |
| `RhsOpt` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `Rhs` |
| `RhsOpt` | `EOL`, `SEMICOLON` | `ε` |
| `SimpleStmt` | `VAL` | `ValStmt` |
| `SimpleStmt` | `VAR` | `VarStmt` |
| `SimpleStmt` | `RETURN` | `ReturnStmt` |
| `SimpleStmt` | `ID` | `ID StmtIdTail` |
| `SimpleStmt` | `DOUBLE_NUM`, `FALSE_TOKEN`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `STRING_LITERAL`, `TRUE_TOKEN` | `ExprStmt` |
| `StmtIdTail` | `ASSIGN` | `ASSIGN Rhs` |
| `StmtIdTail` | `AND_TOKEN`, `BY`, `DIV`, `DOT`, `EOL`, `EQ`, `GE`, `GT`, `LE`, `LPAREN`, `LT`, `MATCH`, `MINUS`, `MUL`, `NEQ`, `OR_TOKEN`, `PLUS`, `PLUSPLUS`, `SEMICOLON`, `TO`, `UNTIL` | `ExprFromId` |
| `StmtRest` | `EOL` | `EOL StmtRest` |
| `StmtRest` | `DOUBLE_NUM`, `FALSE_TOKEN`, `FOR`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `StmtRest` | `DEDENT` | `ε` |
| `Type` | `BOOLEAN_TOKEN`, `DOUBLE_TOKEN`, `ID`, `INT_TOKEN`, `LIST_TOKEN`, `STRING_TOKEN`, `UNIT_TOKEN` | `TypeAtom TypeUnionRest` |
| `TypeAtom` | `INT_TOKEN` | `INT_TOKEN` |
| `TypeAtom` | `DOUBLE_TOKEN` | `DOUBLE_TOKEN` |
| `TypeAtom` | `STRING_TOKEN` | `STRING_TOKEN` |
| `TypeAtom` | `UNIT_TOKEN` | `UNIT_TOKEN` |
| `TypeAtom` | `BOOLEAN_TOKEN` | `BOOLEAN_TOKEN` |
| `TypeAtom` | `LIST_TOKEN` | `LIST_TOKEN LBRACK Type RBRACK` |
| `TypeAtom` | `ID` | `ID` |
| `TypeDef` | `TYPE` | `TYPE ID ASSIGN Type EOL` |
| `TypeUnionRest` | `PIPE` | `PIPE TypeAtom TypeUnionRest` |
| `TypeUnionRest` | `ASSIGN`, `COMMA`, `EOL`, `RBRACK`, `RPAREN` | `ε` |
| `ValStmt` | `VAL` | `VAL ID ValVarTail` |
| `ValVarTail` | `COLON` | `COLON Type ASSIGN Rhs` |
| `ValVarTail` | `ASSIGN` | `ASSIGN Rhs` |
| `VarStmt` | `VAR` | `VAR ID ValVarTail` |
| `WhileStmt` | `WHILE` | `WHILE Expr DO EOL INDENT Block DEDENT` |
