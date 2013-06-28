/*
 * Copyright (C) 2011 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "exceptions.h"

SE_BEGIN_CXX

/**
 * implement syncevolution exception handler
 * to cover its default implementation
 */
DBUS_MESSAGE_TYPE* SyncEvoHandleException(DBUS_MESSAGE_TYPE *msg)
{
    /** give an opportunity to let syncevolution handle exception */
    Exception::handle();
    try {
        throw;
    } catch (const GDBusCXX::dbus_error &ex) {
        return DBUS_NEW_ERROR_MSG(msg, ex.dbusName().c_str(), "%s", ex.what());
    } catch (const GDBusCXX::DBusCXXException &ex) {
        return DBUS_NEW_ERROR_MSG(msg, ex.getName().c_str(), "%s", ex.getMessage());
    } catch (const std::runtime_error &ex) {
        return DBUS_NEW_ERROR_MSG(msg, "org.syncevolution.Exception", "%s", ex.what());
    } catch (...) {
        return DBUS_NEW_ERROR_MSG(msg, "org.syncevolution.Exception", "unknown");
    }
}

SE_END_CXX
