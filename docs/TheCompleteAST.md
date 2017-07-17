# The Complete AST
The AST that is created by the parser is not 'Complete'. To complete it, a number of tasks must be done to simplify
it. The final AST is then verified by the `CompleteASTVerifier` pass, which will error if the AST does not meet the
following conditions:
* The `prev` field of every node must be either `nullptr` or point to another valud `ASTNode`
* The `next` field of every node must be either `nullptr` or point to another valid `ASTNode`
* The `type` field of every node must point to a valid `TypeRef`
* If the `shouldFreeTypeRef` field is `true`, the `TypeRef` in `type` must not be referenced by any other node
* The `containingScope` field of every node must point to a valid `ScopeDef`
* The operands of every `ConditionNode` must have at **most** one `ConstantNode`, as some platforms are unable to
  compare two immediates (these should be constant-folded and the condition done at compile-time)
* The `condition` fields of every `BranchNode` and `WhileNode` must be valid `ConditionNode`s. Other forms of
  conditionals such as a `ConstantNode<bool>` should be evaluated at compile-time and the condition removed)
