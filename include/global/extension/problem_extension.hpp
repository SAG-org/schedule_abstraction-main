#ifndef PROBLEM_EXTENSION_HPP
#define PROBLEM_EXTENSION_HPP

#include <vector>
#include <memory>

namespace NP
{
	namespace Global {
		
		// Base class for problem extension
		struct Problem_extension {
			virtual ~Problem_extension() = default;
		};

		class Problem_extensions
		{
		public:
			Problem_extensions() = default;
			virtual ~Problem_extensions() = default;

			// Register an extension
			size_t register_extension(std::unique_ptr<Problem_extension> extension) {
				extensions.push_back(std::move(extension));
				return extensions.size() - 1; // Return the index of the registered extension
			}

			// Register an extension by type
			template<typename Extension, typename... Args>
			inline size_t register_extension(Args&&... args) {
				return register_extension(std::make_unique<Extension>(std::forward<Args>(args)...));
			}

			// Get an extension by ID
			template<typename Extension>
			Extension* get(size_t id) {
				if (id < extensions.size()) {
					return static_cast<Extension*>(extensions[id].get());
				}
				return nullptr; // Out of bounds
			}

			// Get an extension by type
			template<typename Extension>
			Extension* get() const {
				for (auto& ext : extensions) {
					if (auto casted = dynamic_cast<Extension*>(ext.get())) {
						return casted;
					}
				}
				return nullptr; // Not found
			}

		private:
			std::vector<std::unique_ptr<Problem_extension>> extensions;
		};

	} // namespace Global
} // namespace NP

#endif // !PROBLEM_EXTENSION_HPP
