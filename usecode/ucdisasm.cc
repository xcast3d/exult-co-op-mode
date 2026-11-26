/*
 *  ucdisasm.cc - Disassembled usecode trace
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "game.h"
#include "ignore_unused_variable_warning.h"
#include "span.h"
#include "stackframe.h"
#include "ucfunction.h"
#include "opcodes.h"
#include "ucinternal.h"
#include "uctools.h"
#include "useval.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::strlen;

int Usecode_internal::get_opcode_length(int opcode) {
	if (opcode >= 0 && static_cast<unsigned>(opcode) < opcode_table.size()) {
		return opcode_table[opcode].nbytes + 1;
	}
	return 0;
}

void Usecode_internal::uc_trace_disasm(Stack_frame* frame) {
	uc_trace_disasm(
			frame->locals, frame->num_args + frame->num_vars, frame->function->statics, frame->data, frame->externs, frame->code,
			frame->ip);
}

void Usecode_internal::uc_trace_disasm(
		Usecode_value* locals, int num_locals, std::vector<Usecode_value>& locstatics, const uint8* data, const uint8* externals,
		const uint8* code, const uint8* ip) {
	ignore_unused_variable_warning(num_locals);
	const int func_ip = static_cast<int>(ip - code);
	const int opcode  = Read1(ip);
	// const uint8 *param_ip = ip;
	const opcode_desc* pdesc = nullptr;

	if (opcode >= 0 && static_cast<unsigned>(opcode) < opcode_table.size()) {
		pdesc = &(opcode_table[opcode]);
	}

	std::printf("      %04X: ", func_ip);

	if (pdesc && pdesc->mnemonic) {
		std::printf("%s", pdesc->mnemonic);
		if (strlen(pdesc->mnemonic) < 4) {
			std::printf("\t");
		}
	} else {
		std::printf("<unknown>");
	}

	if (pdesc && pdesc->nbytes > 0) {
		uint16       varref;
		signed short immed;
		int          offset;
		switch (pdesc->type) {
		case op_byte:
			std::printf("\t%02x", Read1(ip));
			break;
		case op_immed:
			// Print immediate operand
			immed = little_endian::Read2(ip);
			std::printf("\t%04hXH\t\t; %d", immed, immed);
			break;
		case op_immed32: {
			const int immed32 = little_endian::Read4s(ip);
			std::printf("\t%08XH\t\t; %d", immed32, immed32);
			break;
		}
		case op_data_string:
		case op_data_string32: {
			const char* pstr;
			size_t      len;
			// Print data string operand
			if (pdesc->type == op_data_string) {
				offset = little_endian::Read2(ip);
			} else {
				offset = little_endian::Read4s(ip);
			}

			pstr = reinterpret_cast<const char*>(data + offset);
			len  = strlen(pstr);
			if (len > 20) {
				len = 20 - 3;
			}
			std::printf("\tL%04X\t\t; ", offset);
			for (unsigned i = 0; i < len; i++) {
				std::printf("%c", pstr[i]);
			}
			if (len < strlen(pstr)) {
				// String truncated
				std::printf("...");
			}
		} break;
		case op_relative_jump:
			// Print jump desination
			offset = little_endian::Read2(ip);
			std::printf("\t%04X", (offset + func_ip + 1 + pdesc->nbytes) & 0xFFFF);
			break;
		case op_relative_jump32:
			offset = little_endian::Read4s(ip);
			std::printf("\t%04X", offset + func_ip + 1 + pdesc->nbytes);
			break;
		case op_sloop:
			if (pdesc->nbytes == 11) {
				ip++;
			}
			little_endian::Read2(ip);
			little_endian::Read2(ip);
			little_endian::Read2(ip);
			varref = little_endian::Read2(ip);
			offset = little_endian::Read2(ip);
			std::printf("\t[%04X], %04X\t= ", varref, (offset + func_ip + 1 + pdesc->nbytes) & 0xFFFF);
			locals[varref].print(cout, true);    // print value (short format)
			break;
		case op_sloop32:
			if (pdesc->nbytes == 13) {
				ip++;
			}
			little_endian::Read2(ip);
			little_endian::Read2(ip);
			little_endian::Read2(ip);
			varref = little_endian::Read2(ip);
			offset = little_endian::Read4s(ip);
			std::printf("\t[%04X], %04X\t= ", varref, offset + func_ip + 1 + pdesc->nbytes);
			locals[varref].print(cout, true);    // print value (short format)
			break;
		case op_immed_and_relative_jump:
			immed  = little_endian::Read2(ip);
			offset = little_endian::Read2(ip);
			std::printf("\t%04hXH, %04X", immed, (offset + func_ip + 1 + pdesc->nbytes) & 0xFFFF);
			break;
		case op_immedreljump32:
			immed  = little_endian::Read2(ip);
			offset = little_endian::Read4s(ip);
			std::printf("\t%04hXH, %04X", immed, offset + func_ip + 1 + pdesc->nbytes);
			break;
		case op_call: {
			const unsigned short func = little_endian::Read2(ip);
			immed                     = Read1(ip);
			tcb::span<const std::string_view> func_table;
			if (Game::get_game_type() == SERPENT_ISLE) {
				if (Game::is_si_beta()) {
					func_table = sibeta_intrinsic_table;
				} else {
					func_table = si_intrinsic_table;
				}
			} else {
				func_table = bg_intrinsic_table;
			}
			std::printf("\t_%s@%d\t; %04X", func_table[func].data(), immed, func);
		} break;
		case op_extcall: {
			// Print extern call
			offset = little_endian::Read2(ip);
			std::printf("\t[%04X]\t\t; %04XH", offset, externals[2 * offset] + 256 * externals[2 * offset + 1]);
			break;
		}
		case op_varref:
			// Print variable reference
			varref = little_endian::Read2(ip);
			std::printf("\t[%04X]\t\t= ", varref);
			locals[varref].print(cout, true);    // print value (short)
			break;
		case op_flgref:
			// Print global flag reference
			offset = little_endian::Read2(ip);
			std::printf("\tflag:[%04X]\t= ", offset);
			if (get_global_flag(offset).value_or(false)) {
				std::printf("set");
			} else {
				std::printf("unset");
			}
			break;
		case op_staticref: {
			const sint16 staticref = little_endian::Read2(ip);
			std::printf("[%04X]\t= ", static_cast<uint16>(staticref));
			if (staticref < 0) {
				// global
				if (static_cast<unsigned>(-staticref) < statics.size()) {
					statics[-staticref].print(cout, true);
				} else {
					std::printf("<out of range>");
				}
			} else {
				if (static_cast<unsigned>(staticref) < locstatics.size()) {
					locstatics[staticref].print(cout, true);
				} else {
					std::printf("<out of range>");
				}
			}
			break;
		}
		default:
			// Unknown type
			break;
		}
	}

	// special cases:
	switch (opcode & 0x7f) {
	case 0x05: {    // JNE/JNE32.
		Usecode_value val;
		if (sp <= stack) {
			val = Usecode_value(0);
		} else {
			val = *(sp - 1);
		}

		if (val.is_false()) {
			std::printf("\t\t(jump taken)");
		} else {
			std::printf("\t\t(jump not taken)");
		}
		break;
	}

		/*  maybe predict this?
		case 0x07:       // CMPS/CMPS32.
		{
			int cnt = little_endian::Read2(param_ip);    // # strings.
			bool matched = false;

			// only try to match if we haven't found an answer yet
			while (!matched && !found_answer && cnt-- > 0) {
				Usecode_value s;
				if (sp + cnt <= stack)
					s = Usecode_value(0);
				else
					s = *(sp - cnt - 1);
				const char *str = s.get_str_value();
				if (str && strcmp(str, user_choice) == 0) {
					matched = true;
					found_answer = true;
				}
			}
			if (val.is_false())
				std::printf("\t\t(jump taken)");
			else
				std::printf("\t\t(jump not taken)");
			break;
		}
		break;
		*/
	}

	std::printf("\n");
}

/*
 *  Quest of the Companions (2026-11-25): the Black Gate shops that sell
 *  lockpicks (Budo 0x85A, Dell 0x874, Greg 0x899, Jehanne 0x8A1) sell the
 *  keyring (shape 1100, from the Keyring mod) too.  Each shop builds
 *  parallel lists (names, shapes, prices, articles and so on), each ending
 *  with "nothing"; a keyring goes in before that, copied from the
 *  backpack's entry (the torch's, where there is no backpack) with its own
 *  name, shape and price.  A function is changed only if it is exactly as
 *  expected; otherwise that shop is left as it was.
 */

namespace {
	constexpr int friend_keyring_shape = 1100;
	constexpr int friend_keyring_price = 20;

	// One instruction of a 16-bit function: where it is, and where its jump
	// offset is (-1: none).
	struct Friend_ins {
		int pc;
		int len;
		int jump_at;
	};

	bool Friend_decode(const unsigned char* code, int start, int end, std::vector<Friend_ins>& out) {
		for (int pc = start; pc < end;) {
			const unsigned op = code[pc];
			if (op >= opcode_table.size() || !opcode_table[op].mnemonic || op >= 0x80) {
				return false;    // Unknown, or 32-bit: not one of the old shops.
			}
			const opcode_desc& desc = opcode_table[op];
			const int          len  = 1 + desc.nbytes;
			int                jump = -1;
			switch (desc.type) {
			case op_relative_jump:
			case op_unconditional_jump:
				jump = pc + 1;
				break;
			case op_argnum_reljump:
			case op_immed_and_relative_jump:
				jump = pc + 3;
				break;
			case op_sloop:
			case op_static_sloop:
				jump = pc + len - 2;
				break;
			default:
				break;
			}
			if (pc + len > end) {
				return false;
			}
			out.push_back({pc, len, jump});
			pc += len;
		}
		return true;
	}

	int Friend_read2(const unsigned char* p) {
		return p[0] | (p[1] << 8);
	}
}    // namespace

void Usecode_internal::friend_shops_sell_keyrings() {
	for (const int funcid : {0x85A, 0x874, 0x899, 0x8A1}) {
		Usecode_function* fun = find_function(funcid);
		if (!fun || fun->extended || fun->len < 8) {
			continue;
		}
		const unsigned char* code     = fun->code;
		const int            data_len = Friend_read2(code);
		const int            header   = 2 + data_len;    // nargs, nlocals, nexterns, externs.
		if (header + 6 > fun->len) {
			continue;
		}
		const int start = header + 6 + 2 * Friend_read2(code + header + 4);
		std::vector<Friend_ins> ins;
		if (start > fun->len || !Friend_decode(code, start, fun->len, ins)) {
			cerr << "Friend Mode: shop 0x" << std::hex << funcid << std::dec << " not as expected; no keyring" << endl;
			continue;
		}
		auto string_at = [&](int offset) -> std::string {
			if (offset < 0 || offset >= data_len) {
				return {};
			}
			const char* text = reinterpret_cast<const char*>(code + 2 + offset);
			return std::string(text, strnlen(text, data_len - offset));
		};
		// The lists: an ARRC of n made from n PUSHS or PUSHI just before it.
		struct List {
			size_t           arrc;     // Index in 'ins'.
			bool             text;     // Strings (else numbers).
			std::vector<int> items;    // Indices in 'ins' of the pushes.
		};
		std::vector<List> lists;
		for (size_t i = 0; i < ins.size(); ++i) {
			if (code[ins[i].pc] != UC_ARRC) {
				continue;
			}
			const int n = Friend_read2(code + ins[i].pc + 1);
			if (n < 2 || static_cast<size_t>(n) > i) {
				continue;
			}
			List list{i, code[ins[i - 1].pc] == UC_PUSHS, {}};
			for (size_t k = i - n; k < i; ++k) {
				if (code[ins[k].pc] != (list.text ? UC_PUSHS : UC_PUSHI)) {
					list.items.clear();
					break;
				}
				list.items.push_back(static_cast<int>(k));
			}
			if (!list.items.empty()) {
				lists.push_back(std::move(list));
			}
		}
		// Each shop's lists: from the names (with a lockpick in them) on,
		// while the lists are as long.
		std::vector<std::vector<List>> shops;
		auto sells_lockpicks = [&](const List& list) {
			if (!list.text) {
				return false;
			}
			for (const int item : list.items) {
				std::string name = string_at(Friend_read2(code + ins[item].pc + 1));
				for (char& c : name) {
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
				if (name.rfind("lockpick", 0) == 0) {
					return true;
				}
			}
			return false;
		};
		for (size_t l = 0; l < lists.size(); ++l) {
			if (!sells_lockpicks(lists[l])) {
				continue;
			}
			// Up to the next shop's names (Budo has two price lists).
			std::vector<List> shop{lists[l]};
			for (size_t m = l + 1; m < lists.size() && lists[m].items.size() == lists[l].items.size()
								   && !sells_lockpicks(lists[m]);
				 ++m) {
				shop.push_back(lists[m]);
			}
			shops.push_back(std::move(shop));
		}
		// What goes in, before each list's last item: (position, bytes).
		std::vector<std::pair<int, std::vector<unsigned char>>> inserts;
		std::string                                              names;    // New strings, after the old.
		bool                                                     ok = !shops.empty();
		for (const std::vector<List>& shop : shops) {
			const size_t n      = shop[0].items.size();
			auto         number = [&](const List& list, size_t k) {
                return Friend_read2(code + ins[list.items[k]].pc + 1);
			};
			// The shapes: the first list of numbers with the lockpick (627).
			const List* shapes = nullptr;
			size_t      lists_before_prices = 0;
			for (size_t m = 1; m < shop.size() && !shapes; ++m) {
				if (!shop[m].text) {
					for (size_t k = 0; k < n; ++k) {
						if (number(shop[m], k) == 627) {
							shapes              = &shop[m];
							lists_before_prices = m;
						}
					}
				}
			}
			if (!shapes || number(*shapes, n - 1) != 0) {
				ok = false;
				break;
			}
			// The entry to copy: the backpack's, else the torch's.
			size_t model = n;
			for (const int shape : {801, 595}) {
				for (size_t k = 0; k + 1 < n && model == n; ++k) {
					if (number(*shapes, k) == shape) {
						model = k;
					}
				}
			}
			// The prices: the next list of numbers that isn't all frames (0xfe99).
			const List* prices = nullptr;
			for (size_t m = lists_before_prices + 1; m < shop.size() && !prices; ++m) {
				if (shop[m].text) {
					continue;
				}
				bool frames = true;
				for (size_t k = 0; k < n; ++k) {
					frames = frames && number(shop[m], k) == 0xfe99;
				}
				if (!frames) {
					prices = &shop[m];
				}
			}
			if (model == n || !prices) {
				ok = false;
				break;
			}
			// "Lockpick" or "lockpick": the keyring's name likewise.
			const std::string model_name = string_at(Friend_read2(code + ins[shop[0].items[model]].pc + 1));
			const char*       keyring    = !model_name.empty() && std::isupper(static_cast<unsigned char>(model_name[0]))
												   ? "Keyring"
												   : "keyring";
			const int name_offset = data_len + static_cast<int>(names.size());
			names += keyring;
			names += '\0';
			for (const List& list : shop) {
				std::vector<unsigned char> push(3);
				push[0]   = list.text ? UC_PUSHS : UC_PUSHI;
				int value = &list == &shop[0] ? name_offset
							: &list == shapes ? friend_keyring_shape
							: &list == prices ? friend_keyring_price
											  : Friend_read2(code + ins[list.items[model]].pc + 1);
				push[1]   = static_cast<unsigned char>(value & 0xff);
				push[2]   = static_cast<unsigned char>((value >> 8) & 0xff);
				inserts.emplace_back(ins[list.items[n - 1]].pc, std::move(push));
			}
		}
		if (!ok) {
			cerr << "Friend Mode: shop 0x" << std::hex << funcid << std::dec << " not as expected; no keyring" << endl;
			continue;
		}
		// Where an old position goes, with the new bytes in.
		auto moved = [&](int pc) {
			int shift = 0;
			for (const auto& insert : inserts) {
				if (insert.first <= pc) {
					shift += static_cast<int>(insert.second.size());
				}
			}
			return pc + shift;
		};
		// The new code: the strings grown, the lists one longer, the jumps
		// over them adjusted.
		std::vector<unsigned char> out;
		const int                  new_data_len = data_len + static_cast<int>(names.size());
		if (new_data_len > 0xffff) {
			continue;
		}
		out.push_back(static_cast<unsigned char>(new_data_len & 0xff));
		out.push_back(static_cast<unsigned char>(new_data_len >> 8));
		out.insert(out.end(), code + 2, code + 2 + data_len);
		out.insert(out.end(), names.begin(), names.end());
		out.insert(out.end(), code + header, code + start);
		bool      fits      = true;
		for (const Friend_ins& one : ins) {
			for (const auto& insert : inserts) {
				if (insert.first == one.pc) {
					out.insert(out.end(), insert.second.begin(), insert.second.end());
				}
			}
			const size_t at = out.size();
			out.insert(out.end(), code + one.pc, code + one.pc + one.len);
			if (code[one.pc] == UC_ARRC) {
				for (const auto& insert : inserts) {
					// This list's last item is just before it.
					if (insert.first == one.pc - 3) {
						const int n = Friend_read2(code + one.pc + 1) + 1;
						out[at + 1] = static_cast<unsigned char>(n & 0xff);
						out[at + 2] = static_cast<unsigned char>(n >> 8);
					}
				}
			}
			if (one.jump_at >= 0) {
				const int old_end    = one.pc + one.len;
				const int rel        = static_cast<sint16>(Friend_read2(code + one.jump_at));
				const int new_target = moved(old_end + rel);
				const int new_rel    = new_target - moved(old_end);
				if (new_rel < -32768 || new_rel > 32767) {
					fits = false;
				}
				const size_t field = at + (one.jump_at - one.pc);
				out[field]         = static_cast<unsigned char>(new_rel & 0xff);
				out[field + 1]     = static_cast<unsigned char>((new_rel >> 8) & 0xff);
			}
		}
		if (!fits || out.size() > 0xffff) {
			continue;
		}
		delete[] fun->code;
		fun->len  = static_cast<int>(out.size());
		fun->code = new unsigned char[out.size()];
		std::copy(out.begin(), out.end(), fun->code);
		cout << "Friend Mode: shop 0x" << std::hex << funcid << std::dec << " sells keyrings" << endl;
#ifdef FRIEND_TEST_KEYS
		// Test builds: QOTC_TEST_SHOP_DUMP=<file> writes the changed shops
		// as a usecode file, for tools/friend-mode/ucread.py.
		if (const char* dump = std::getenv("QOTC_TEST_SHOP_DUMP")) {
			if (FILE* file = std::fopen(dump, "ab")) {
				const unsigned char head[4] = {static_cast<unsigned char>(funcid & 0xff), static_cast<unsigned char>(funcid >> 8),
											   static_cast<unsigned char>(fun->len & 0xff), static_cast<unsigned char>(fun->len >> 8)};
				std::fwrite(head, 1, 4, file);
				std::fwrite(fun->code, 1, fun->len, file);
				std::fclose(file);
			}
		}
#endif
	}
}
