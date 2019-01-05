#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>
#include <typeinfo>
#include "CodeObject.h"
#include "InstructionDecoder.h"
#include "CFG.h"

using namespace std;
using namespace Dyninst;
using namespace ParseAPI;

using namespace InstructionAPI;

int main(int argc, char **argv) {
	if(argc != 2) {
		printf("Usage: %s <binary path>\n", argv[0]);
		return -1;
	}

	char* binaryPath = argv[1];

	SymtabCodeSource *sts;
	CodeObject *co;
	Instruction::Ptr instr;
	SymtabAPI::Symtab *symTab;
	std::string binaryPathStr(binaryPath);

	bool isParsable = SymtabAPI::Symtab::openFile(symTab, binaryPathStr);

	if(isParsable == false) {
		const char *error = "error: file cannot be parsed";
		cout << error;
		return -1;
	}

	sts = new SymtabCodeSource(binaryPath);
	co = new CodeObject(sts);
	//parse the binary given as a command line arg
	co->parse();

	// get list of all functions in the binary
	const CodeObject::funclist &all = co->funcs();
	if(all.size() == 0) {
		const char* error = "error: no functions in file";
		cout << error;
		return -1;
	}

	auto fit = all.begin();
	Function *f = *fit;

	// create an Instruction decoder which will convert the binary opcodes to strings
	InstructionDecoder decoder(f->isrc()->getPtrToInstruction(f->addr()),
			InstructionDecoder::maxInstructionLength,
			f->region()->getArch());
	for(;fit != all.end(); ++fit) {
		Function *f = *fit;

		// output the start address of this function
		cout << "0x" << hex << f->addr() << ": " << f->name() << endl;
		// prepare the first level loops
		vector<LoopTreeNode*> children = f->getLoopTree()->children;

		uint8_t loop_depth = 1;
		uint8_t next_depth_loop_num = 0;		
		uint8_t curr_depth_loop_num = children.size();

		queue<LoopTreeNode*> loops;

		for(int l = 0; l < curr_depth_loop_num; l++) {
			loops.push(children[l]);
		}

		// create an Instruction deocder which will convert the binary opcodes to strings
		InstructionDecoder decoder(f->isrc()->getPtrToInstruction(f->addr()), InstructionDecoder::maxInstructionLength, f->region()->getArch());

		// run BFS and track tree depth (loop depth)
		while(!loops.empty()) {
			LoopTreeNode* loop_node = loops.front();
			loops.pop();
			Loop* loop = loop_node->loop;

			cout << "\t" << unsigned(loop_depth) << ": " << loop_node->name() << "\t" << endl;
				

			// enqueue all children for current node (these have a loop depth of loop_depth+1)
			children = loop_node->children;
			for(int l = 0; l < children.size(); l++) {
				loops.push(children[l]);
			}

			// get all basic blocks for current loop
			vector<Block*> basic_blocks;
			if(loop->getLoopBasicBlocksExclusive(basic_blocks)) {
				for(int b = 0; b < basic_blocks.size(); b++) {
					Block* curr_block = basic_blocks[b];
					Address curr_addr = curr_block->start();
					Address end_addr = curr_block->last();

					// if current BBL has zero instruction, don't output it
					if(curr_addr == end_addr) {
						continue;
					}

					// check all instructions in the current BBL for readMemory
					while(curr_addr < end_addr) {
						// decode current instruction
						instr = decoder.decode(
								(unsigned char*) f->isrc()->getPtrToInstruction(curr_addr));

						// capture read instructions only
						if(instr->readsMemory()) {
							cout << "\t\t0x" << hex << curr_addr;
							cout << ": \"" << instr->format() << "\"" << endl;
							// get to the address of the next instruction
						}
						curr_addr += instr->size();
					}
				}
			}

			curr_depth_loop_num--;
			next_depth_loop_num += children.size();

			// update loop_depth
			if(curr_depth_loop_num == 0) {
				curr_depth_loop_num = next_depth_loop_num;
				loop_depth++;
				next_depth_loop_num = 0;
			}
		}
		cout << endl << endl;
	}
	return 0;
}
