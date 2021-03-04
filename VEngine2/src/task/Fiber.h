#pragma once

namespace task
{
	class Fiber
	{
	public:
		typedef void(__stdcall *FiberFunction)(void *fiberParameter);

		static Fiber convertThreadToFiber(void *fiberParameter) noexcept;
		explicit Fiber(FiberFunction fiberFunction, void *fiberParameter) noexcept;
		explicit Fiber(void *fiberHandle = nullptr, void *fiberParameter = nullptr, bool createdFromThread = false) noexcept;
		Fiber(const Fiber &) = delete;
		Fiber &operator= (const Fiber &) = delete;
		Fiber(Fiber &&fiber) noexcept;
		Fiber &operator=(Fiber &&fiber) noexcept;
		~Fiber();
		void switchToFiber(const Fiber &fiber) const noexcept;
		void *getFiberData() const noexcept;

	private:
		void *m_fiberHandle = nullptr;
		void *m_fiberParameter = nullptr;
		bool m_createdFromThread = false;;
	};
}