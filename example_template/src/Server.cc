// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Server.h"

#include "version.h"

Server::Server(std::string appName)
: ctk::Application(appName), config{this, "Configuration", getName() + "-Config.xml"}, device{this,
                                                                                           "MappedDummyDevice"},
  templateModule{this, "TemplateModule", "This is a template module, adapt as needed!"} {
  std::cout << "*** Construction of " << appName << " in version " << AppVersion::major << "." << AppVersion::minor
            << "." << AppVersion::patch << " starts. ***" << std::endl;
  ctk::setDMapFilePath(getName() + ".dmap");
  std::cout << "*** Construction of " << appName << " in version " << AppVersion::major << "." << AppVersion::minor
            << "." << AppVersion::patch << " done. ***" << std::endl;
}

Server::~Server() {
  shutdown();
}
