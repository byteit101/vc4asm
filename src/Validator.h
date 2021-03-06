/*
 * Validator.h
 *
 *  Created on: 23.11.2014
 *      Author: mueller
 */

#ifndef VALIDATOR_H_
#define VALIDATOR_H_

#include "Inst.h"
#include "utils.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <climits>
using namespace std;

/// Helper class to validate VideoCore IV instruction constraints.
class Validator
{public:
	/// Use this address as absolute address of the first instruction word.
	/// @remarks The absolute address is used for comments and to resolve absolute branch targets.
	uint32_t BaseAddr = 0;
 private:
	/// Maximum number of instructions where constraints apply.
	/// @remarks This is related to the pipeline length in QPU elements.
	/// It is used to reparse the constraints at branch targets.
	enum { MAX_DEPEND = 4 };
	/// Singleton constant to indicate that an instruction of this type had not yet been identified.
	/// @remarks This is just a very unlikely value.
	enum { NEVER = -0x40000000 };

	/// @brief Type to store integer values for each register in a regfile including I/O register.
	/// Index: [B!A][number].
	/// @details The first index selects regfile A/B. The second index the register number.
	/// @par For peripherals registers that are mapped to both register files
	/// the entries for register file A also track register file B access.
	/// The entries in [1] are unused in this case.
	typedef int regfile_t[2][64];
	/// @brief Validation worker state.
	/// @details The state saves all information about the current validator.
	/// It needs to be clonable to follow both paths at conditional branch instructions.
	struct state
	{	/// This work item is invoked from a branch from this location.
		/// 0 in case of the initial start address.
		const int From;
		/// Start instruction where the parsing started.
		const int Start;
		/// @brief Register used as target of the last QPU vector rotation of the MUL ALU.
		/// -1 in case none so far.
		/// @details The value stored is compatible to Inst::WaddrM.
		int LastRotReg = -1;
		/// Last instruction that caused an load to register r4 by a signal (not SFU writes).
		int LastLDr4 = NEVER;
		/// Last instruction that was of type branch.
		int LastBRANCH = NEVER;
		/// Last thread end instruction.
		int LastTEND = NEVER;
		/// @brief Last register file read access at instruction #
		regfile_t LastRreg;
		/// @brief Last register file write access at instruction #
		regfile_t LastWreg;
		/// Create initial state.
		/// @param start First instruction to check. In doubt 0.
		state(int start);
		/// Revoke copy constructor.
		state(const state& r) = delete;
		/// Clone validator state.
		/// @param r State to clone
		/// @param at Clone is initiated from this instruction.
		/// @param target Further verification should start from here.
		/// @details The information about origin and target is used to relocate the last instruction fields.
		/// @remarks This constructor is intended for branch instructions.
		state(const state& r, int at, int target);
	};
	/// List of validator states. The list owns the states exclusively.
	typedef vector<unique_ptr<state>> workitems_t;
 private:
	/// @brief Bit vector of instructions that have already been checked.
	/// @details This is used to avoid redundant checks when code branches join.
	vector<bool> Done;
	/// @brief List of validations to be done.
	/// @details This list grows when branch instructions are processed
	/// and it shrinks as the items are processed.
	workitems_t WorkItems;
	// some fields are copied from *WorkItems.back() for efficiency reasons.
	int  From;        ///< Branch source, 0 in case of the initial start address.
	int  Start;       ///< Start instruction where the parsing started.
	int  At;          ///< Current Instruction.
	int  To;          ///< End analysis here.
	bool Pass2;       ///< Check only for dependencies of branch target.
	Inst Instruct;    ///< current instruction to analyze.
 private:
	/// @brief print a validation warning.
	/// @param refloc The Validation error at the current instruction (\ref At) is caused by the instruction at location \p refloc.
	/// The location is automatically relocated if it is from before a branch.
	/// @param fmt Message format string
	/// @remarks The reference locations before the branch have always instruction numbers less than start
	/// because the constructor relocated the accordingly. This function does the opposite transform to get meaningful messages.
	void Message(int refloc, const char* fmt, ...) PRINTFATTR(3);
	/// Convert an ALU input multiplexer to an index into a write register index.
	/// @remarks This function is used to check for back to back read after write.
	int  FromMux(Inst::mux m);
	/// Ensure termination of the current task.
	/// @param Maximum number of further instructions to check.
	void TerminateRq(int after);
	/// Execute a validation task for the given code block.
	/// @param instructions Code block to check.
	/// @param st (initial) state of the validator. This is also used to specify the region to be checked.
	void ProcessItem(const vector<uint64_t>& instructions, state& st);
 public:
	/// Validate a code block. The validation starts always at the first instruction.
	/// @param instructions Code block to check.
	void Validate(const vector<uint64_t>& instructions);
};

#endif // VALIDATOR_H_
