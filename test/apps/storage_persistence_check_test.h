#pragma once

#include "storage.h"

namespace sandbox::apps {

class StoragePersistenceCheckTest {
	public:
		void init();
		void update();

	private:
		Storage storage_{};
		bool initialized_ = false;
		bool completed_ = false;
};

}  // namespace sandbox::apps
