/**
 * ApplicationCore.h
 *
 *  Created on: Jun 14, 2016
 *      Author: Martin Hierholzer
 *
 *  This is the main header file for the ApplicationCore library. It brings all
 * includes and functionality needed for writing an application.
 */

#ifndef APPLICATION_CORE_CONVENIENCE_H
#define APPLICATION_CORE_CONVENIENCE_H

#include "Application.h"
#include "ApplicationModule.h"
#include "ArrayAccessor.h"
#include "ConfigReader.h"
#include "ControlSystemModule.h"
#include "DeviceModule.h"
#include "HierarchyModifyingGroup.h"
#include "ModuleGroup.h"
#include "ScalarAccessor.h"
#include "VariableGroup.h"
#include "VirtualModule.h"
#include "VoidAccessor.h"

#include <ChimeraTK/Utilities.h> // for ChimeraTK::setDMapFilePath(), which is used by all applications

#endif /* APPLICATION_CORE_CONVENIENCE_H */
