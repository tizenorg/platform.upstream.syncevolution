/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
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

#ifndef INCL_EVOLUTION_ABSTRACT_CONFIG_NODE
# define INCL_EVOLUTION_ABSTRACT_CONFIG_NODE

#include <map>
#include <utility>
#include <string>
#include <sstream>
using namespace std;

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/predicate.hpp>

/**
 * This class corresponds to the Funambol C++ client
 * DeviceManagementNode, but offers a slightly different API.  See
 * ConfigTree for details.
 */
class ConfigNode {
 public:
    /** free resources without saving */
    virtual ~ConfigNode() {}

    /** creates a file-backed config node which accepts arbitrary key/value pairs */
    static boost::shared_ptr<ConfigNode> createFileNode(const string &filename);

    /** a name for the node that the user can understand */
    virtual string getName() const = 0;

    /**
     * save all changes persistently
     */
    virtual void flush() = 0;

    /**
     * Returns the value of the given property
     *
     * @param property - the property name
     * @return value of the property or empty string if not set
     */
    virtual string readProperty(const string &property) const = 0;

    /**
     * Sets a property value.
     *
     * @param property   the property name
     * @param value      the property value (zero terminated string)
     * @param comment    a comment explaining what the property is about, with
     *                   \n separating lines; might be used by the backend 
     *                   when adding a new property
     * @param defValue   If a defValue is provided and the value
     *                   matches the default, then the node is asked to
     *                   remember that the value hasn't really been changed.
     *                   An implementation can decide to not support this.
     */
    virtual void setProperty(const string &property,
                             const string &value,
                             const string &comment = string(""),
                             const string *defValue = NULL) = 0;

    /**
     * Sets a boolean property, using "true/false".
     */
    void setProperty(const string &property, bool value) {
        setProperty(property, value ? "true" : "false");
    }

    /**
     * Sets a property value with automatic conversion to the underlying string,
     * using stream formatting.
     */
    template <class T> void setProperty(const string &property,
                                        const T &value) {
        std::stringstream strval;
        strval << value;
        setProperty(property, strval.str());
    }

    bool getProperty(const string &property,
                     string &value) {
        value = readProperty(property);
        return !value.empty();
    }

    bool getProperty(const string &property,
                     bool &value) {
        std::string str = readProperty(property);
        if (str.empty()) {
            return false;
        }

        /* accept keywords */
        if (boost::iequals(str, "true") ||
            boost::iequals(str, "yes") ||
            boost::iequals(str, "on")) {
            value = true;
            return true;
        }
        if (boost::iequals(str, "false") ||
            boost::iequals(str, "no") ||
            boost::iequals(str, "off")) {
            value = false;
            return true;
        }

        /* zero means false */
        double number;
        if (getProperty(property, number)) {
            value = number != 0;
            return true;
        }

        return false;
    }

    template <class T> bool getProperty(const string &property,
                                        T &value) {
        std::string str = readProperty(property);
        if (str.empty()) {
            return false;
        } else {
            std::stringstream strval(str);
            strval >> value;
            return !strval.bad();
        }
    }


    /**
     * Extract all list of all currently defined properties
     * and their values. Does not include values which were
     * initialized with their defaults, if the implementation
     * remembers that.
     *
     * @retval props    to be filled with key/value pairs; guaranteed
     *                  to be empty before the call
     */
    virtual void readProperties(map<string, string> &props) const = 0;
    typedef map<string, string> PropsType;

    /**
     * Remove a certain property.
     *
     * @param property    the name of the property which is to be removed
     */
    virtual void removeProperty(const string &property) = 0;

    /**
     * Node exists in backend storage.
     */
    virtual bool exists() const = 0;
};

#endif