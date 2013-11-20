/*
 * Copyright (C) 2012 Intel Corporation
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

/**
 * Abstract definition of sorting and searching plugin. Used
 * by folks.cpp, must be provided by exactly one implementation
 * which is chosen at compile time.
 */

#ifndef INCL_SYNCEVO_DBUS_SERVER_PIM_LOCALE_FACTORY
#define INCL_SYNCEVO_DBUS_SERVER_PIM_LOCALE_FACTORY

#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>

#include <folks/folks.h>

#include <syncevo/util.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

class IndividualCompare;
class IndividualFilter;

/**
 * Normalized phone numbers (E164). A subset of the full
 * i18n::phonenumbers::PhoneNumber data.
 */
class SimpleE164
{
 public:
    SimpleE164() : m_countryCode(0), m_nationalNumber(0) {}

    typedef int32_t CountryCode_t;
    typedef uint64_t NationalNumber_t;

    /**
     * Country code (for example, 49 for the +49 prefix of Germany),
     * 0 if unknown/unset.
     */
    CountryCode_t m_countryCode;

    /**
     * Phone number after the country code as it would appear in E.164
     * after the +<country> prefix, again given as binary value,
     * 0 if unknown/unset.
     */
    NationalNumber_t m_nationalNumber;

    /**
     * +<country><national> if country code and national number are set,
     * +<country> if only country code is set,
     * <national> if only national number is set,
     * empty string if both are unset.
     */
    std::string toString() const;

    bool operator == (const SimpleE164 &other) const 
    {
        return m_countryCode == other.m_countryCode &&
        m_nationalNumber == other.m_nationalNumber;
    }
    bool operator != (const SimpleE164 &other) const { return !(*this == other); }
};

/**
 * Factory for everything related to the current locale: sorting and
 * searching.
 */
class LocaleFactory
{
 public:
    /**
     * Exactly one factory can be created, chosen at compile time.
     */
    static boost::shared_ptr<LocaleFactory> createFactory();

    /**
     * Creates a compare instance or throws an error when that is not
     * possible.
     *
     * @param order     factory-specific string which chooses one of
     *                  the orderings supported by the factory
     * @return a valid instance, must not be NULL
     */
    virtual boost::shared_ptr<IndividualCompare> createCompare(const std::string &order) = 0;

    /**
     * A recursive definition of a search expression.
     * All operand names, field names and values are strings.
     */
    typedef boost::make_recursive_variant<
        std::string,
        std::vector< boost::recursive_variant_ >
        >::type Filter_t;

    /**
     * Simplified JSON representation (= no escaping of special characters),
     * for debugging and error reporting.
     */
    static std::string Filter2String(const Filter_t &filter);

    /**
     * Throws "expected <item>, got instead: <filter as string>" when
     * conversion to V fails.
     */
    static const std::string &getFilterString(const Filter_t &filter, const char *expected);
    static const std::vector<Filter_t> &getFilterArray(const Filter_t &filter, const char *expected);

    /**
     * Creates a filter instance or throws an error when that is not
     * possible.
     *
     * @param  represents a (sub-)filter
     * @level  0 at the root of the filter, incremented by one for each
     *         non-trivial indirection; i.e., [ [ <filter> ] ] still
     *         treats <filter> as if it was the root search
     *
     * @return a valid instance, must not be NULL
     */
    virtual boost::shared_ptr<IndividualFilter> createFilter(const Filter_t &filter, int level) = 0;

    /**
     * To be called when parsing a Filter_t caused an exception.
     * Will add information about the filter and a preamble, if
     * called at the top level.
     */
    static void handleFilterException(const Filter_t &filter, int level, const std::string *file, int line);

    /**
     * Pre-computed data for a single FolksIndividual which will be needed
     * for searching. Strictly speaking, this should be an opaque pointer
     * whose content is entirely owned by the implementation of LocaleFactory.
     * For the sake of performance and simplicity, we define a struct instead
     * which can be embedded inside IndividualData. Leads to better memory
     * locality and reduces overall memory consumption/usage.
     */
    struct Precomputed
    {
        typedef std::vector<SimpleE164> PhoneNumbers;
        PhoneNumbers m_phoneNumbers;
        bool operator == (const Precomputed &other) const;
        bool operator != (const Precomputed &other) const { return !(*this == other); }
    };

    /**
     * (Re)set pre-computed data for an individual.
     */
    virtual bool precompute(FolksIndividual *individual, Precomputed &precomputed) const = 0;
};

SE_END_CXX

#endif // INCL_SYNCEVO_DBUS_SERVER_PIM_LOCALE_FACTORY
