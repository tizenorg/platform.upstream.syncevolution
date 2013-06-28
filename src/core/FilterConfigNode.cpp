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

#include "FilterConfigNode.h"
#include "EvolutionSyncClient.h"

#include <boost/foreach.hpp>
#include <boost/algorithm/string/join.hpp>

FilterConfigNode::FilterConfigNode(const boost::shared_ptr<ConfigNode> &node,
                                   const ConfigFilter &filter) :
    m_filter(filter),
    m_node(node),
    m_readOnlyNode(node)
{
}

FilterConfigNode::FilterConfigNode(const boost::shared_ptr<const ConfigNode> &node,
                                   const ConfigFilter &filter) :
    m_filter(filter),
    m_readOnlyNode(node)
{
}

void FilterConfigNode::addFilter(const string &property,
                                 const string &value)
{
    m_filter[property] = value;
}

void FilterConfigNode::setFilter(const ConfigFilter &filter)
{
    m_filter = filter;
}

string FilterConfigNode::readProperty(const string &property) const
{
    ConfigFilter::const_iterator it = m_filter.find(property);

    if (it != m_filter.end()) {
        return it->second;
    } else {
        return m_readOnlyNode->readProperty(property);
    }
}

void FilterConfigNode::setProperty(const string &property,
                                   const string &value,
                                   const string &comment,
                                   const string *defValue)
{
    ConfigFilter::iterator it = m_filter.find(property);

    if (!m_node.get()) {
        EvolutionSyncClient::throwError(getName() + ": read-only, setting properties not allowed");
    }

    if (it != m_filter.end()) {
        m_filter.erase(it);
    }
    m_node->setProperty(property, value, comment, defValue);
}

void FilterConfigNode::readProperties(map<string, string> &props) const
{
    m_readOnlyNode->readProperties(props);

    BOOST_FOREACH(const StringPair &filter, m_filter) {
        props.insert(filter);
    }
}

void FilterConfigNode::removeProperty(const string &property)
{
    ConfigFilter::iterator it = m_filter.find(property);

    if (!m_node.get()) {
        EvolutionSyncClient::throwError(getName() + ": read-only, removing properties not allowed");
    }

    if (it != m_filter.end()) {
        m_filter.erase(it);
    }
    m_node->removeProperty(property);
}

void FilterConfigNode::flush()
{
    if (!m_node.get()) {
        EvolutionSyncClient::throwError(getName() + ": read-only, flushing not allowed");
    }
    m_node->flush();
}

FilterConfigNode::ConfigFilter::operator string () const {
    vector<string> res;

    BOOST_FOREACH(const StringPair &filter, *this) {
        res.push_back(filter.first + " = " + filter.second);
    }
    sort(res.begin(), res.end());
    return boost::join(res, "\n");
}