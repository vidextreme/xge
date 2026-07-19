#pragma once
namespace xg
{
	struct System
	{
		XG_DECLARE_BASE_SYSTEM(System)
	public:
		virtual ~System() = default;

		virtual void Initialize() {}
		virtual void Shutdown() {}
	};
}