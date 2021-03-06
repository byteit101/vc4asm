/*
 * Eval.h
 *
 *  Created on: 16.11.2014
 *      Author: mueller
 */

#ifndef EVAL_H_
#define EVAL_H_

#include "expr.h"
#include <vector>


/// @brief Worker class for expression parser stack
/// @details
/// - Instantiate this class.
/// - Push the values and operators with PushValue and PushOperator in the order they appear.
/// - Call \ref Evaluate to get the final expression value.
/// @par The class takes care of the operator precedence.
/// Precedence and evaluation order are the same as for the C language.
/// @par If something went wrong Eval::Fail with an error message is thrown.
/// It derives from std::string which carries the message.
class Eval
{public:
	/// @brief Operators
	/// @details The high nibble is the precedence. Bit 2 is set for unary operators.
	enum mathOp : unsigned char
	{	BRO  = 0xe4 ///< \c ( : opening brace
	,	NOP  = 0xd4 ///< unary \c + : no operation
	,	NEG  = 0xd5 ///< unary \c - : negative value
	,	NOT  = 0xd6 ///< \c ~ : 2s complement
	,	lNOT = 0xd7 ///< \c ! : logical not
	,	POW  = 0xc0 ///< \c ** : power
	,	MUL  = 0xb0 ///< \c * : multiply
	,	DIV  = 0xb1 ///< \c / : divide
	,	MOD  = 0xb2 ///< \c % : modulus
	,	ADD  = 0xa0 ///< \c + : add
	,	SUB  = 0xa1 ///< binary \c - : subtract
	,	ASL  = 0x90 ///< \c << : arithmetic shift left
	,	ASR  = 0x91 ///< \c >> : arithmetic shift right
	,	SHL  = 0x92 ///< \c <<< : logical shift left
	,	SHR  = 0x93 ///< \c >>> : logical shift right
	,	GT   = 0x80 ///< \c > : greater than
	,	GE   = 0x81 ///< \c >= : greater or equal
	,	LT   = 0x82 ///< \c < : less than
	,	LE   = 0x83 ///< \c <= : less or equal
	,	EQ   = 0x70 ///< \c == : equal
	,	NE   = 0x71 ///< \c != : not equal
	,	AND  = 0x60 ///< \c & : bitwise and
	,	XOR  = 0x51 ///< \c ^ : bitwise exclusive or
	,	OR   = 0x42 ///< \c | : bitwise or
	,	lAND = 0x30 ///< \c && : logical and
	,	lXOR = 0x21 ///< \c ^^ : logical exclusive or
	,	lOR  = 0x12 ///< \c || : logical or
	,	BRC  = 0x00 ///< \c ) : closing brace
	,	EVAL = 0x01 ///< done : evaluate all
	};
	/// Exception thrown when evaluation Fails
	/// @remarks This is only a typed string so far.
	/// Future versions might add additional error information.
	struct Fail : string
	{	Fail(const char* fmt, ...);
	};
 private:
	/// Bits encoded in the mathOp enum values.
	enum
	{	UNARY_OP  = 0x04 ///< Unary operator flag.
	,	PRECEDENCE= 0xf0 ///< Precedence mask.
	};

	/// @brief Entry in the evaluation stack.
	/// @details This structure consists of a value and an operator.
	/// The value is logically first. It is unused in case of unary operators
	/// and must be of type V_NONE in this case.
	struct exprEntry : public exprValue
	{	mathOp    Op;    ///< Operator, NOP by default
		exprEntry() : Op(NOP) {}
	};
	/// @brief Evaluation stack
	/// @details This vector contains at least one element which is initially { V_NONE, NOP }.
	/// When Operators or values are added the fields are filled. After each operator added
	/// a new initial entry is added at the end of the vector.
	/// @par Except for intermediate states the evaluation stack will always contain
	/// the operators strictly monotonic ordered by precedence.
	/// If an operator with lower precedence is added the stack is evaluated immediately
	/// until the condition is met again. I.e. all operators with higher or same precedence
	/// than the one to be added are evaluated back to front.
	vector<exprEntry> Stack;

	/// Helper class to evaluate an operator on the stack.
	class operate
	{	/// Right hand side callstack entry.
		exprEntry& rhs;
		/// Left hand side callstack entry. The operator to evaluate is always taken from this one.
		exprEntry& lhs;
		/// @brief Union of the types of the operands.
		/// @details The value stored is 2**valueType. For each operand a bit is set in this bit vector.
		/// Except for unary operators. In this case only the \ref rhs value contributes.
		/// @remarks This field is used to check for type compatibility more fast.
		unsigned  types;
		/// Raise an error messages because the current operator is not applicable
		/// to the expression types provided.
		/// @exception Fail The function always throws.
		void      TypesFail();
		/// Check whether all operands are of type integer (V_INT).
		/// Raise an error otherwise.
		/// @exception Fail At least one operand is not an integer.
		void      CheckInt();
		/// @brief Check whether both operand types implicitly convert to bool.
		/// @exception Fail At least one of the operands is not convertible to boolean,
		/// e.g. a label or a register.
		/// @details The function also patches the return type to be integer (V_INT)
		/// since vc4asm has no boolean type.
		void      CheckBool();
		/// Propagate integer operands to float.
		/// @details If one of the operands is of type integer (V_INT)
		/// then it is immediately converted to float. This may cause a loss of precision.
		/// Furthermore the conversion cannot deal with unsigned integers.
		/// @pre Both operands must be either of type V_INT or V_FLOAT.
		void      PropFloat();
		/// @brief Check whether both operands are either of type integer (V_INT) or float (V_FLOAT)
		/// and propagate the other operand to float if one of them is already of type float.
		/// @exception Fail At least one of the operands is not of type V_INT or V_FLOAT.
		/// @details The propagation to float may cause a loss of precision.
		/// Furthermore the conversion cannot deal with unsigned integers.
		/// @post Both operands are either of type V_INT or V_FLOAT.
		void      CheckNumericPropFloat();
		/// @brief Generic comparison of the two operands
		/// and turn the return type into V_INT.
		/// @return
		/// - -1 : \ref lhs is less than \ref rhs
		/// - 0 : both oprands are equal
		/// - 1 : \ref lhs is greater than \ref rhs
		/// @details The comparison operators are quite permissive with respect to operand types.
		/// You will not get a type error regardless what you compare.
		/// To get a strict weak ordering the following the following rules apply.
		/// - All numeric types behave as you might suggest.
		/// - Integer values are taken as signed.
		/// - Load per QPU element constants are compared by their immediate value in the QPU instruction.
		/// - Labels compare by their order of appearance.
		/// - Register expressions are first compared by their type (see regType),
		///   secondly by the QPU element rotation factor and last
		///   by their number.
		/// - All operands of different types except for V_INT versus V_FLOAT
		///   compare in the following ascending order:
		///   numeric values are less than per element constants,
		///   per element constants are less than registers
		///   registers are less than labels.
		///   Different types never compare equal, except for V_INT vs. V_FLOAT of course.
		int       Compare();
	 public:
		/// @brief Prepare evaluation of the last but one operator.
		/// @details Evaluate the operator of the last but one stack entry
		/// with last but one value as left hand side (unless the operator is unary)
		/// and the last value as right hand side.
		operate(vector<exprEntry>& stack);
		/// @brief Perform the evaluation if the last operator has lower precedence
		/// than the the last but one operator.
		/// @param unary Evaluate unary operators only.
		/// @return true if the operation is finished without success.
		/// false if an operator is evaluated and more operators might be ready for evaluation.
		/// @brief The evaluation stores the result in the last but one value
		/// and moves the last operator back to the last but one entry.
		/// It is up to the caller to pop the last element from the stack
		/// when the function returned false.
		bool      Apply(bool unary);
	};
 private:
	/// Evaluates everything in the call stack with higher or same precedence than the most recently added operator.
	/// @param unary true: evaluate unary operators only, i.e. the last object pushed is a value.
	/// @return - true: evaluation done
	/// - false: Operator was a closing brace but it does not belong to the current expression because of no matching opening brace.
	/// This happens when parsing the last argument to a function.
	bool        partialEvaluate(bool unary);
 public:
	/// Create an empty evaluation stack.
	            Eval() { Stack.emplace_back(); }
	/// @brief Push an operator on the evaluation stack.
	/// @param op Operator to push.
	/// If this is a binary operator the last object pushed on the stack must be a value or a closing brace.
	/// But the function is smart enough to convert the binary operators ADD/SUB to NOP/NEG
	/// if no value is on the stack. So the parser need not to distinguish between unary and binary +/-.
	/// You can just pass the code for the binary operators only. (But not the other way around.)
	/// @exception Fail Something went wrong, error message.
	/// It is up to the parser to enrich this message with the source code location.
	/// @details The function implicitly evaluates all operators with lower or same precedence
	/// before the new operator is added.
	bool        PushOperator(mathOp op);
	/// @brief Push a value on the evaluation stack.
	/// @param value Expression value. In general any expression type could be used.
	/// But there are restrictions for each operator witch expression type they accept.
	/// @return Normally true. Only false if the operator was a closing brace
	/// that does not belong to the current expression. This is a hack to allow function argument parsers to stop without an error
	/// when they hit a closing brace that is not part of the expression but belongs to the function call operator instead.
	/// I.e. you should immediately stop parsing this expression and not consider the last brace to be parsed.
	/// @exception Fail Something went wrong, error message.
	/// It is up to the parser to enrich this message with the source code location.
	/// @details This will immediately evaluate all unary operators on top of the stack.
	void        PushValue(const exprValue& value);
	/// @brief Evaluate the expression.
	/// @details This functions enforces all operators on the call stack to evaluate.
	/// The expression must be complete to succeed, i.e no unmatched opening brace should be left
	/// and no trailing operator should be on the stack.
	/// @return Return value from the last operator evaluated.
	/// @exception Fail Something went wrong, error message.
	/// It is up to the parser to enrich this message with the source code location.
	exprValue   Evaluate();
 public:
	/// Human readable string for an operator.
	static const char* op2string(mathOp op);
};

#endif // EVAL_H_
