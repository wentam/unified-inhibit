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
#include "util.hpp"

#define THIS UserCommandsInhibitor

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
           std::function<void(Inhibitor*,Inhibit)> unInhibitCB,
           Args args) : Inhibitor(inhibitCB, unInhibitCB, "user-commands")
{
  // TODO test with empty strings as commands, make sure no crashy

  if (args.params.contains("inhibit-action"))
    cmds.insert({InhibitType::NONE, strMerge(args.params.at("inhibit-action"),' ')});
  else if (args.params.contains("ia"))
    cmds.insert({InhibitType::NONE, strMerge(args.params.at("ia"),' ')});

  if (args.params.contains("uninhibit-action"))
    uncmds.insert({InhibitType::NONE, strMerge(args.params.at("uninhibit-action"),' ')});
  else if (args.params.contains("uia"))
    uncmds.insert({InhibitType::NONE, strMerge(args.params.at("uia"),' ')});

  for (auto t : inhibitTypeStrings()) {
    if (args.params.contains(t+"-inhibit-action")) {
      cmds.insert({stringToInhibitType(t), strMerge(args.params.at(t+"-inhibit-action"),' ')});
    }
    if (args.params.contains(t+"-ia")) {
      cmds.insert({stringToInhibitType(t), strMerge(args.params.at(t+"-ia"),' ')});
    }
    if (args.params.contains(t+"-uninhibit-action")) {
      uncmds.insert({stringToInhibitType(t), strMerge(args.params.at(t+"-uninhibit-action"),' ')});
    }
    if (args.params.contains(t+"-uia")) {
      uncmds.insert({stringToInhibitType(t), strMerge(args.params.at(t+"-uia"),' ')});
    }
  }

  int64_t cmdSize = cmds.size()+uncmds.size();

  if (cmdSize == 0) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] User actions: "
           "No user commands specified. See man page to specify (un)inhibit actions. \n");
  } else if (cmdSize > 0) {
    printf("[" ANSI_COLOR_GREEN "->" ANSI_COLOR_RESET "] User actions: "
           "%ld user action(s) present\n", cmdSize);
    this->ok = true;
  }
}

Inhibitor::ReturnObject THIS::start() {
  while(1) co_await std::suspend_always();
}

void THIS::handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) {
  if (!this->ok) return;

  for (auto t : inhibitTypes()) {
    if ((inhibited & t) != (lastInhibited & t)) {
      if ((inhibited & t) > 0) {
        if (this->cmds.contains(t)) {
          printf("Running %s inhibit command: %s\n",
                 inhibitTypeToString(t).c_str(),
                 this->cmds.at(t).c_str());
          [[maybe_unused]] int r = system(this->cmds.at(t).c_str());
        }
      } else {
        if (this->uncmds.contains(t)) {
          printf("Running %s uninhibit command: %s\n",
                 inhibitTypeToString(t).c_str(),
                 this->uncmds.at(t).c_str());
          [[maybe_unused]] int r = system(this->uncmds.at(t).c_str());
        }
      }
    }
  }

  if (lastInhibited != inhibited) {
    if (cmds.contains(InhibitType::NONE) && inhibited != InhibitType::NONE) {
      printf("Running inhibit command: %s\n", this->cmds.at(InhibitType::NONE).c_str());
      [[maybe_unused]] int r = system(this->cmds.at(InhibitType::NONE).c_str());
    }
    if (uncmds.contains(InhibitType::NONE) && inhibited == InhibitType::NONE) {
      printf("Running uninhibit command: %s\n", this->uncmds.at(InhibitType::NONE).c_str());
      [[maybe_unused]] int r = system(this->uncmds.at(InhibitType::NONE).c_str());
    }
  }

  lastInhibited = inhibited;
};

Inhibit THIS::doInhibit(InhibitRequest r) {
  Inhibit ret = {};
  ret.type = r.type;
  ret.appname = r.appname;
  ret.reason = r.reason;
  ret.id = {};
  ret.created = time(NULL);
  return ret;
}
