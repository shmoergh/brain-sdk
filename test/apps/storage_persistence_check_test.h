#pragma once

namespace sandbox::apps {

class StoragePersistenceCheckTest {
	public:
		void init();
		void update();

	private:
		bool initialized_ = false;
		bool completed_ = false;
};

}  // namespace sandbox::apps
