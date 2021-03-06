//-----------------------------------------------------------------------------
/** @file libboardgame_util/IntervalChecker.cpp
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#include "IntervalChecker.h"

#include <limits>
#include "Assert.h"

namespace libboardgame_util {

//-----------------------------------------------------------------------------

IntervalChecker::IntervalChecker(TimeSource& time_source, double time_interval,
                                 const function<bool()>& f)
    : m_time_source(time_source),
      m_time_interval(time_interval),
      m_function(f)
{
    LIBBOARDGAME_ASSERT(time_interval > 0);
}

bool IntervalChecker::check_expensive()
{
    if (m_result)
        return true;
    if (m_is_deterministic)
    {
        m_result = m_function();
        m_count = m_count_interval;
        return m_result;
    }
    double time = m_time_source();
    if (! m_is_first_check)
    {

        double diff = time - m_last_time;
        double adjust_factor;
        if (diff == 0)
            adjust_factor = 10;
        else
        {
            adjust_factor = m_time_interval / diff;
            if (adjust_factor > 10)
                adjust_factor = 10;
            else if (adjust_factor < 0.1)
                adjust_factor = 0.1;
        }
        double new_count_interval = adjust_factor * double(m_count_interval);
        if (new_count_interval > double(numeric_limits<unsigned>::max()))
            m_count_interval = numeric_limits<unsigned>::max();
        else if (new_count_interval < 1)
            m_count_interval = 1;
        else
            m_count_interval = static_cast<unsigned>(new_count_interval);
        m_result = m_function();
    }
    else
    {
        m_is_first_check = false;
    }
    m_last_time = time;
    m_count = m_count_interval;
    return m_result;
}

void IntervalChecker::set_deterministic(unsigned interval)
{
    LIBBOARDGAME_ASSERT(interval >= 1);
    m_is_deterministic = true;
    m_count = interval;
    m_count_interval = interval;
}

//-----------------------------------------------------------------------------

} // namespace libboardgame_util
