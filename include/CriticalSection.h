#include <util.h>

class CriticalSection
{
	public:
		CriticalSection()
		{
			_wereInterruptsEnabled = disable_irq();
		}

		~CriticalSection()
		{
			if (_wereInterruptsEnabled)
			{
				enable_irq();
			}
		}

	private:
		bool _wereInterruptsEnabled;
};
