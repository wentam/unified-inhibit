// Copyright (C) 2022 Matthew Egeler
//
// This file is part of unified-inhibit.
//
// unified-inhibit is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// unified-inhibit is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with unified-inhibit. If
// not, see <https://www.gnu.org/licenses/>.

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
		this->callEvent(true, ii);

		return ii;
	}

	void Inhibitor::unInhibit(InhibitID id)	{
		if (this->activeInhibits.contains(id)) {
			auto mid = this->activeInhibits.at(id);
			this->doUnInhibit(id);	
			this->activeInhibits.erase(id);
			this->callEvent(false, mid);
		}
		// TODO: else throw exception?
	}

	void Inhibitor::registerInhibit(Inhibit& i) {
		activeInhibits.insert({i.id, i});
		this->inhibitCB(this, i);
		this->callEvent(true, i);
	}

	void Inhibitor::registerUnInhibit(InhibitID& id) {
		if (this->activeInhibits.contains(id)) {
			auto mid = this->activeInhibits.at(id);
			this->activeInhibits.erase(id);
			this->unInhibitCB(this, mid);
			this->callEvent(false, mid);
		}
	}

	void Inhibitor::callEvent(bool isInhibit, Inhibit i) {
		if (isInhibit) this->handleInhibitEvent(i);
		else           this->handleUnInhibitEvent(i);

		InhibitType inhibited = this->inhibited();
		if (inhibited != this->lastInhibitState) {
			this->handleInhibitStateChanged(inhibited, i);
			this->lastInhibitState = inhibited;
		}
	}
} // End namespace uinhibit
