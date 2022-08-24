// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Server.h"

#include "version.h"

void Server::defineConnections() {
  ctk::setDMapFilePath("devices.dmap");

  std::cout << "****************************************************************" << std::endl;
  std::cout << "*** Template server version " << AppVersion::major << "." << AppVersion::minor << "."
            << AppVersion::patch << std::endl;

  dev.connectTo(cs /*, timer.tick*/);
  config.connectTo(cs);

  dumpConnectionGraph();
  dumpGraph();
  dumpModuleGraph("module-graph.dot");
}
