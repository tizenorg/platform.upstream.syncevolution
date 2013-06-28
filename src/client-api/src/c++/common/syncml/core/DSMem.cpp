/*
 * Funambol is a mobile platform developed by Funambol, Inc. 
 * Copyright (C) 2003 - 2007 Funambol, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License version 3 as published by
 * the Free Software Foundation with the addition of the following permission 
 * added to Section 15 as permitted in Section 7(a): FOR ANY PART OF THE COVERED
 * WORK IN WHICH THE COPYRIGHT IS OWNED BY FUNAMBOL, FUNAMBOL DISCLAIMS THE 
 * WARRANTY OF NON INFRINGEMENT  OF THIRD PARTY RIGHTS.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Affero General Public License 
 * along with this program; if not, see http://www.gnu.org/licenses or write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 * 
 * You can contact Funambol, Inc. headquarters at 643 Bair Island Road, Suite 
 * 305, Redwood City, CA 94063, USA, or at email address info@funambol.com.
 * 
 * The interactive user interfaces in modified source and object code versions
 * of this program must display Appropriate Legal Notices, as required under
 * Section 5 of the GNU Affero General Public License version 3.
 * 
 * In accordance with Section 7(b) of the GNU Affero General Public License
 * version 3, these Appropriate Legal Notices must retain the display of the
 * "Powered by Funambol" logo. If the display of the logo is not reasonably 
 * feasible for technical reasons, the Appropriate Legal Notices must display
 * the words "Powered by Funambol".
 */


#include "syncml/core/DSMem.h"
#include "base/globalsdef.h"

USE_NAMESPACE


DSMem::DSMem() {
    sharedMem      = false;
    long    maxMem = 0;
    long    maxID  = 0;
}
DSMem::~DSMem() {
    sharedMem      = false;
    long    maxMem = 0;
    long    maxID  = 0;
}

/**
 * Creates a new DSMem object with the given sharedMem, maxMem and maxID
 *
 * @param sharedMem is true if the datastore uses shared memory
 * @param maxMem the maximum memory size for o given datastore
 * @param maxID the maximum number of items that can be stored in a given
 *              datastore
 *
 */
DSMem::DSMem(bool sharedMem, long maxMem, long maxID) {
    setMaxMem(maxMem);
    setMaxID(maxID);
    this->sharedMem = sharedMem;
}

/**
 * Returns the memoryShared status
 *
 * @return <i>true</i> if the datastore memory is shared, <i>false</i> otherwise
 */
bool DSMem::isSharedMem() {
    return (sharedMem != NULL);
}
/**
 * Sets the memoryShared status
 *
 * @param sharedMem the new memoryShared status
 */
void DSMem::setSharedMem(bool sharedMem) {
    this->sharedMem = sharedMem;
}

/**
 * Gets Boolean shared memory
 *
 * @return sharedMem the Boolean sharedMem
 */
bool DSMem::getSharedMem() {

    return sharedMem;
}

/**
 * Gets the maximum memory size in bytes
 *
 * @return if value is -1 indicates that the property value is unspecified
 */
long DSMem::getMaxMem() {
    return maxMem;
}

/**
 * Sets the max memory property
 *
 * @param maxMem the value of max memory property
 *
 */
void DSMem::setMaxMem(long maxMem) {
    this->maxMem = maxMem;
}

/**
 * Gets the maximum number of items
 *
 * @return if value is -1 indicates that the property value is unspecified
 */
long DSMem::getMaxID() {
    return maxID;
}

/**
 * Sets the max ID property
 *
 * @param maxID the value of maxID property
 */
void DSMem::setMaxID(long maxID) {
    this->maxID = maxID;
}

DSMem* DSMem::clone() {
    DSMem* ret = new DSMem(sharedMem, maxMem, maxID);
    return ret;
}
