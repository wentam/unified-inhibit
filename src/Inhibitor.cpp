#include "Inhibitor.hpp"

static std::mutex lastInstanceIdMutex;
static uint64_t lastInstanceId = 0;

namespace uinhibit {

	Inhibitor::Inhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
											 std::function<void(Inhibitor*, Inhibit)> unInhibitCB) : 
		inhibitCB(inhibitCB), unInhibitCB(unInhibitCB) {
			lastInstanceIdMutex.lock();
			this->instanceId = lastInstanceId++;
			lastInstanceIdMutex.unlock();
		};

	InhibitType Inhibitor::inhibited() {
		InhibitType ret = InhibitType::NONE;
		for (auto& [id, inhibit] : this->activeInhibits) ret = static_cast<InhibitType>(ret | inhibit.type);
		return ret;
	}

	Inhibit Inhibitor::inhibit(InhibitRequest i)	{
		auto ii = this->doInhibit(i);	
		activeInhibits.insert({ii.id, ii});
		this->callEvent(true);

		return ii;
	}

	void Inhibitor::unInhibit(InhibitID id)	{
		if (this->activeInhibits.contains(id)) {
			this->doUnInhibit(id);	
			this->activeInhibits.erase(id);
			this->callEvent(false);
		}
		// TODO: else throw exception?
	}

	void Inhibitor::registerInhibit(Inhibit& i) {
		activeInhibits.insert({i.id, i});
		this->inhibitCB(this, i);
		this->callEvent(true);
	}

	void Inhibitor::registerUnInhibit(InhibitID& id) {
		if (this->activeInhibits.contains(id)) {
			auto mid = this->activeInhibits.at(id);
			this->activeInhibits.erase(id);
			this->unInhibitCB(this, mid);
			this->callEvent(false);
		}
	}

	void Inhibitor::callEvent(bool isInhibit) {
		if (isInhibit) this->handleInhibitEvent();
		else           this->handleUnInhibitEvent();

		InhibitType inhibited = this->inhibited();
		if (inhibited != this->lastInhibitState) {
			this->handleInhibitStateChanged(inhibited);
			this->lastInhibitState = inhibited;
		}
	}
} // End namespace uinhibit
