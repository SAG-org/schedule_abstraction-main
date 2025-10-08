#ifndef MK_FIRM_HPP
#define MK_FIRM_HPP

#include <cstdint>
#include <cassert>
#include <vector>
#include <string>
#include <unordered_map>
#include <bit>
#include "io.hpp"

namespace NP {
    namespace Global {
        namespace MK_analysis {

            // MK constraint parameters for each task
            struct MK_constraint {
				unsigned int misses;
                unsigned int window_length;
            };

			using Task_index = unsigned long;
			using MK_constraints = std::unordered_map<Task_index, MK_constraint>;

			// Sliding window to track the number of misses in the last 'window_size' job completions
            template<class Time>
            class Misses_window {
				// Using bitsets to store 32 bits representing misses in the window
                typedef uint32_t Window;
				Window certain_misses;
				Window potential_misses;
                // max window size is 31
				size_t window_size;
				// number of misses in the window
                unsigned int num_certain_misses;
				unsigned int num_potential_misses;
            public:
                Misses_window(size_t window_size) 
					: window_size(window_size)
					, certain_misses(0)
					, potential_misses(0)
					, num_certain_misses(0)
					, num_potential_misses(0)
				{
					assert(window_size > 0 && window_size <= 31);
				}

                void add_certain_miss() {
					// if the bit that is shifted out was a hit and we add a miss, increment the number of misses in the window
					if ((certain_misses & (1UL << (window_size - 1))) == 0)
						num_certain_misses++;
					certain_misses <<= 1;
					certain_misses |= 1UL;
					certain_misses &= ~(1UL << window_size); // ensure we only keep 'window_size' bits (erase the most significant bit)
					add_potential_miss();
                }

				void add_potential_miss() {
					// if the bit that is shifted out was a hit and we add a miss, increment the number of misses in the window
					if ((potential_misses & (1UL << (window_size - 1))) == 0)
						num_potential_misses++;
					potential_misses <<= 1;
					potential_misses |= 1UL;
					potential_misses &= ~(1UL << window_size); // ensure we only keep 'window_size' bits (erase the most significant bit)
				}

                void add_certain_hit() {
					// if the bit that is shifted out was a miss and we add a hit, decrement the number of misses in the window
					if ((certain_misses & (1UL << (window_size - 1))))
						num_certain_misses--;
                    certain_misses <<= 1;
					certain_misses &= ~(1UL << window_size); // ensure we only keep 'window_size' bits (erase the most significant bit)
					add_potential_hit();
				}

				void add_potential_hit() {
					// if the bit that is shifted out was a miss and we add a hit, decrement the number of misses in the window
					if ((potential_misses & (1UL << (window_size - 1))))
						num_potential_misses--;
					potential_misses <<= 1;
					potential_misses &= ~(1UL << window_size); // ensure we only keep 'window_size' bits (erase the most significant bit)
				}

                unsigned int get_num_certain_misses() const {
                    return num_certain_misses;
				}

				unsigned int get_num_potential_misses() const {
					return num_potential_misses;
				}

				void merge(const Misses_window<Time>& other) {
					assert(window_size == other.window_size);
					// Merging by taking the worst-case (maximum) number of potential misses in the sliding window
					num_potential_misses = std::max(num_potential_misses, other.num_potential_misses);
					auto and_p_misses = potential_misses & other.potential_misses;
					auto xor_p_misses = potential_misses ^ other.potential_misses;
					auto and_p_count = std::popcount(and_p_misses);
					potential_misses = and_p_misses;
					auto diff = num_potential_misses - and_p_count;
					// for any position where there was a miss in one of the windows, but not both (identified by xor_misses),
					while (diff > 0) {
						assert(xor_p_misses != 0); // we should have enough bits to set
						auto i = std::countr_zero(xor_p_misses);
						// a miss in one of the windows, but not both
						// set the bit to 1 (miss) to be conservative
						potential_misses |= (1UL << i);
						xor_p_misses &= ~(1UL << i); // clear the bit so we don't pick it again
						diff--;
					}

					// Merging by taking the best-case (minimum) number of certain misses in the sliding window
					num_certain_misses = std::min(num_certain_misses, other.num_certain_misses);
					auto and_c_misses = certain_misses & other.certain_misses;
					auto xor_c_misses = certain_misses ^ other.certain_misses;
					auto and_c_count = std::popcount(and_c_misses);
					certain_misses = and_c_misses;
					diff = num_certain_misses - and_c_count;
					// for any position where there was a miss in one of the windows, but not both (identified by xor_misses),
					while (diff > 0) {
						assert(xor_c_misses != 0); // we should have enough bits to set
						auto i = std::countl_zero(xor_c_misses);
						// a miss in one of the windows, but not both
						// set the bit to 1 (miss) to be conservative
						certain_misses |= (1UL << (31 - i));
						xor_c_misses &= ~(1UL << (31 - i)); // clear the bit so we don't pick it again
						diff--;
					}
				}
            };

			inline std::unordered_map<Task_index, MK_constraint> parse_mk_constraints_csv(std::istream& in) {
				std::unordered_map<Task_index, MK_constraint> mk_constraints;
				std::string line;
				// Clear any flags
				in.clear();
				// Move the pointer to the beginning
				in.seekg(0, std::ios::beg);
				// skip column headers
				next_line(in);
				// parse all rows
				while (more_data(in)) {
					// each row contains one mk constraint for a different task
					Task_index task_id;
					unsigned int misses, window_length;
					in >> task_id;
					next_field(in);
					in >> misses;
					next_field(in);
					in >> window_length;
					mk_constraints[task_id] = MK_constraint{ misses, window_length };
					next_line(in);
				}
				return mk_constraints;
			}

			template<class Time>
			void validate_mk_constraints(const std::unordered_map<Task_index, MK_constraint>& mk_constraints, const typename Job<Time>::Job_set& jobs)
			{
				auto num_tasks = mk_constraints.size();
				for (const auto& j : jobs) {
					int c = mk_constraints.count(j.get_task_id());
					if (c == 0)
						throw std::runtime_error("An MK constraint must be defined for each task in the workload");
					if (c > 1)
						throw std::runtime_error("No more than one MK constraint must be defined for each task in the workload");
				}
				for (const auto& c : mk_constraints) {
					if (c.second.window_length < 1 || c.second.window_length > 31)
						throw std::runtime_error("MK window length must be between 1 and 31");
					if (c.second.misses > c.second.window_length)
						throw std::runtime_error("MK misses must be less than or equal to the window length");
				}
			}
        }
    }
}
#endif // MK_FIRM_HPP