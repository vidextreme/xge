#pragma once
namespace xg
{
	struct Subsystem
	{
		XG_DECLARE_BASE_SUBSYSTEM(Subsystem)
	public:
		virtual ~Subsystem() = default;

		virtual void Initialize() {}
		virtual void Shutdown() {}
	};
}