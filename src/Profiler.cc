// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Profiler.h"

namespace ChimeraTK {

  std::list<Profiler::ThreadData*> Profiler::threadDataList;

  std::mutex Profiler::threadDataList_mutex;

} /* namespace ChimeraTK */
