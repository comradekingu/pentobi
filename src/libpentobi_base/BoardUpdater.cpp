//-----------------------------------------------------------------------------
/** @file libpentobi_base/BoardUpdater.cpp */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BoardUpdater.h"

#include "libboardgame_sgf/Util.h"

namespace libpentobi_base {

using namespace std;
using libboardgame_sgf::util::get_path_from_root;

//-----------------------------------------------------------------------------

namespace {

void check_has_not_property(const Node& node, const string& id)
{
    if (node.has_property(id))
        throw Exception(format("Cannot handle property '%s'") % id);
}

} // namespace

//-----------------------------------------------------------------------------

void BoardUpdater::update(const Node& node)
{
    LIBBOARDGAME_ASSERT(m_tree.contains(node));
    m_bd.init();
    get_path_from_root(node, m_path);
    BOOST_FOREACH(const Node* i, m_path)
    {
        // Setup properties are not defined or used in the Pentobi SGF format
        // at the moment, but we don't want to silently ignore them should
        // they be used in the future because then we would misinterpret the
        // board state.
        check_has_not_property(*i, "AB");
        check_has_not_property(*i, "AW");
        check_has_not_property(*i, "A1");
        check_has_not_property(*i, "A2");
        check_has_not_property(*i, "A3");
        check_has_not_property(*i, "A4");
        check_has_not_property(*i, "AE");
        ColorMove mv = m_tree.get_move(*i);
        if (! mv.is_null())
        {
            if (m_bd.get_nu_moves() >= Board::max_game_moves)
                throw Exception("too many moves");
            m_bd.play(mv);
        }
    }
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_base
