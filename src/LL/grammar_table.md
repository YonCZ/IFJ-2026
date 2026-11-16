# LL tabulka

Generováno z `ll_check.py` (263 testů, 0 kolizí). Kombinace neterminál × token, která v tabulce není, = prázdná buňka = syntaktická chyba 2.

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
| `StmtIdTail` | `AND_TOKEN`, `BY`, `DIV`, `DOT`, `EOL`, `EQ`, `GE`, `GT`, `LE`, `LPAREN`, `LT`, `MINUS`, `MUL`, `NEQ`, `OR_TOKEN`, `PLUS`, `PLUSPLUS`, `SEMICOLON`, `TO`, `UNTIL` | `ExprFromId` |
| `StmtRest` | `EOL` | `EOL StmtRest` |
| `StmtRest` | `DOUBLE_NUM`, `FALSE_TOKEN`, `FOR`, `ID`, `IF`, `INTERP_START`, `INT_NUM`, `LIST_TOKEN`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `StmtRest` | `DEDENT` | `ε` |
| `Type` | `INT_TOKEN` | `INT_TOKEN` |
| `Type` | `DOUBLE_TOKEN` | `DOUBLE_TOKEN` |
| `Type` | `STRING_TOKEN` | `STRING_TOKEN` |
| `Type` | `UNIT_TOKEN` | `UNIT_TOKEN` |
| `Type` | `BOOLEAN_TOKEN` | `BOOLEAN_TOKEN` |
| `Type` | `LIST_TOKEN` | `LIST_TOKEN LBRACK Type RBRACK` |
| `ValStmt` | `VAL` | `VAL ID ValVarTail` |
| `ValVarTail` | `COLON` | `COLON Type ASSIGN Rhs` |
| `ValVarTail` | `ASSIGN` | `ASSIGN Rhs` |
| `VarStmt` | `VAR` | `VAR ID ValVarTail` |
| `WhileStmt` | `WHILE` | `WHILE Expr DO EOL INDENT Block DEDENT` |
