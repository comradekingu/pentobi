//-----------------------------------------------------------------------------
/** @file libpentobi_base/Board.cpp */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Board.h"

#include "libboardgame_util/Unused.h"

namespace libpentobi_base {

using namespace std;

//-----------------------------------------------------------------------------

namespace {

void write_x_coord(ostream& out, unsigned int width, unsigned int offset)
{
    for (unsigned int i = 0; i < offset; ++i)
        out << ' ';
    char c = 'A';
    for (unsigned int x = 0; x < width; ++x, ++c)
    {
        if (x < 26)
            out << ' ';
        else
            out << 'a';
        if (x == 26)
            c = 'A';
        out << c;
    }
    out << '\n';
}

void set_color(ostream& out, const char* esc_sequence)
{
    if (Board::color_output)
        out << esc_sequence;
}

} // namespace

//-----------------------------------------------------------------------------

void StartingPoints::add_colored_starting_point(unsigned int x, unsigned int y,
                                                Color c)
{
    Point p(x, y);
    m_is_colored_starting_point[p] = true;
    m_starting_point_color[p] = c;
    m_starting_points[c].push_back(p);
}

void StartingPoints::add_colorless_starting_point(unsigned int x,
                                                  unsigned int y)
{
    Point p(x, y);
    m_is_colorless_starting_point[p] = true;
    m_starting_points[Color(0)].push_back(p);
    m_starting_points[Color(1)].push_back(p);
    m_starting_points[Color(2)].push_back(p);
    m_starting_points[Color(3)].push_back(p);
}

void StartingPoints::init(GameVariant game_variant, const Geometry& geometry)
{
    m_is_colored_starting_point.init(geometry, false);
    m_is_colorless_starting_point.init(geometry, false);
    m_starting_point_color.init(geometry);
    m_starting_points[Color(0)].clear();
    m_starting_points[Color(1)].clear();
    m_starting_points[Color(2)].clear();
    m_starting_points[Color(3)].clear();
    if (game_variant == game_variant_classic
        || game_variant == game_variant_classic_2)
    {
        add_colored_starting_point(0, 19, Color(0));
        add_colored_starting_point(19, 19, Color(1));
        add_colored_starting_point(19, 0, Color(2));
        add_colored_starting_point(0, 0, Color(3));
    }
    else if (game_variant == game_variant_duo)
    {
        add_colored_starting_point(4, 9, Color(0));
        add_colored_starting_point(9, 4, Color(1));
    }
    else if (game_variant == game_variant_trigon
             || game_variant == game_variant_trigon_2)
    {
        add_colorless_starting_point(17, 3);
        add_colorless_starting_point(17, 14);
        add_colorless_starting_point(9, 6);
        add_colorless_starting_point(9, 11);
        add_colorless_starting_point(25, 6);
        add_colorless_starting_point(25, 11);
    }
    else
        LIBBOARDGAME_ASSERT(false);
}

//-----------------------------------------------------------------------------

bool Board::color_output = false;

Board::Board(GameVariant game_variant)
{
    m_color_char[Color(0)] = 'X';
    m_color_char[Color(1)] = 'O';
    m_color_char[Color(2)] = '#';
    m_color_char[Color(3)] = '@';
    init(game_variant);
}

void Board::copy_from(const Board& bd)
{
    init(bd.get_game_variant());
    for (unsigned int i = 0; i < bd.get_nu_moves(); ++i)
        play(bd.get_move(i));
}

void Board::gen_moves(Color c, ArrayList<Move, Move::range>& moves) const
{
    moves.clear();
    bool is_first_move = (m_pieces_left[c].size() == get_nu_pieces());
    if (is_first_move)
    {
        BOOST_FOREACH(Point p, get_starting_points(c))
            if (! m_forbidden[c][p])
                gen_moves(c, p, m_marker, moves);
    }
    else
    {
        for (Iterator i(*this); i; ++i)
            if (is_attach_point(*i, c) && ! m_forbidden[c][*i])
                gen_moves(c, *i, get_adj_status_index(*i, c), m_marker, moves);
    }
    m_marker.clear(moves);
}

void Board::gen_moves(Color c, Point p, MoveMarker& marker,
                      ArrayList<Move, Move::range>& moves) const
{
    BOOST_FOREACH(unsigned int i, m_pieces_left[c])
    {
        BOOST_FOREACH(Move mv, m_board_const->get_moves(i, p))
        {
            if (marker[mv])
                continue;
            if (! is_forbidden(c, mv))
            {
                moves.push_back(mv);
                marker.set(mv);
            }
        }
    }
}

void Board::gen_moves(Color c, Point p, unsigned int adj_status_index,
                      MoveMarker& marker,
                      ArrayList<Move, Move::range>& moves) const
{
    BOOST_FOREACH(unsigned int i, m_pieces_left[c])
    {
        BOOST_FOREACH(Move mv, m_board_const->get_moves(i, p, adj_status_index))
        {
            if (marker[mv])
                continue;
            if (! is_forbidden(c, mv))
            {
                moves.push_back(mv);
                marker.set(mv);
            }
        }
    }
}

unsigned int Board::get_bonus(Color c) const
{
    unsigned int bonus = 0;
    if (m_pieces_left[c].size() == 0)
    {
        bonus = 15;
        for (unsigned int i = get_nu_moves(); i > 0; --i)
        {
            ColorMove mv = get_move(i - 1);
            if (mv.color == c)
            {
                if (! mv.move.is_pass())
                {
                    const MoveInfo& info = get_move_info(mv.move);
                    if (get_piece(info.piece).get_size() == 1)
                        bonus += 5;
                }
                break;
            }
        }
    }
    return bonus;
}

Color Board::get_effective_to_play() const
{
    Color c = m_to_play;
    for (unsigned int i = 0; i < m_nu_colors; ++i)
    {
        if (has_moves(c))
            return c;
        c = c.get_next(m_nu_colors);
    }
    return m_to_play;
}

unsigned int Board::get_points(Color c) const
{
    return m_board_const->get_total_piece_points() - get_points_left(c);
}

unsigned int Board::get_points_with_bonus(Color c) const
{
    return get_points(c) + get_bonus(c);
}

unsigned int Board::get_points_left(Color c) const
{
    unsigned int n = 0;
    BOOST_FOREACH(unsigned int i, m_pieces_left[c])
        n += get_piece(i).get_size();
    return n;
}

int Board::get_score(Color c, double& game_result) const
{
    if (m_game_variant == game_variant_duo)
    {
        unsigned int points0 = get_points_with_bonus(Color(0));
        unsigned int points1 = get_points_with_bonus(Color(1));
        if (c == Color(0))
        {
            if (points0 > points1)
                game_result = 1;
            else if (points0 < points1)
                game_result = 0;
            else
                game_result = 0.5;
            return points0 - points1;
        }
        else
        {
            if (points1 > points0)
                game_result = 1;
            else if (points1 < points0)
                game_result = 0;
            else
                game_result = 0.5;
            return points1 - points0;
        }
    }
    else if (m_game_variant == game_variant_classic
             || m_game_variant == game_variant_trigon)
    {
        unsigned int points = get_points_with_bonus(c);
        int score = 0;
        unsigned int max_opponent_points = 0;
        for (ColorIterator i(m_nu_colors); i; ++i)
            if (*i != c)
            {
                unsigned int points = get_points_with_bonus(*i);
                score -= points;
                max_opponent_points = max(max_opponent_points, points);
            }
        score = score / 3 + points;
        if (points > max_opponent_points)
            game_result = 1;
        else if (points < max_opponent_points)
            game_result = 0;
        else
            game_result = 0.5;
        return score;
    }
    else
    {
        LIBBOARDGAME_ASSERT(m_game_variant == game_variant_classic_2
                            || m_game_variant == game_variant_trigon_2);
        unsigned int points0 =
            get_points_with_bonus(Color(0)) + get_points_with_bonus(Color(2));
        unsigned int points1 =
            get_points_with_bonus(Color(1)) + get_points_with_bonus(Color(3));
        if (c == Color(0) || c == Color(2))
        {
            if (points0 > points1)
                game_result = 1;
            else if (points0 < points1)
                game_result = 0;
            else
                game_result = 0.5;
            return points0 - points1;
        }
        else
        {
            if (points1 > points0)
                game_result = 1;
            else if (points1 < points0)
                game_result = 0;
            else
                game_result = 0.5;
            return points1 - points0;
        }
    }
}

bool Board::has_moves(Color c) const
{
    bool is_first_move = (m_pieces_left[c].size() == get_nu_pieces());
    for (Iterator i(*this); i; ++i)
        if (! m_forbidden[c][*i]
            && (is_attach_point(*i, c)
                || (is_first_move && get_starting_points(c).contains(*i))))
            if (has_moves(c, *i))
                return true;
    return false;
}

bool Board::has_moves(Color c, Point p) const
{
    BOOST_FOREACH(unsigned int i, m_pieces_left[c])
    {
        BOOST_FOREACH(Move mv, m_board_const->get_moves(i, p))
        {
            const MovePoints& points = get_move_points(mv);
            bool is_legal = true;
            BOOST_FOREACH(Point p2, points)
                if (m_forbidden[c][p2])
                {
                    is_legal = false;
                    break;
                }
            if (is_legal)
                return true;
        }
    }
    return false;
}

void Board::init(GameVariant game_variant)
{
    m_game_variant = game_variant;
    if (m_game_variant == game_variant_duo)
    {
        m_color_name[Color(0)] = "Blue";
        m_color_name[Color(1)] = "Green";
        m_color_esc_sequence[Color(0)] = "\x1B[1;34;47m";
        m_color_esc_sequence[Color(1)] = "\x1B[1;32;47m";
        m_color_esc_sequence_text[Color(0)] = "\x1B[1;34m";
        m_color_esc_sequence_text[Color(1)] = "\x1B[1;32m";
    }
    else
    {
        m_color_name[Color(0)] = "Blue";
        m_color_name[Color(1)] = "Yellow";
        m_color_name[Color(2)] = "Red";
        m_color_name[Color(3)] = "Green";
        m_color_esc_sequence[Color(0)] = "\x1B[1;34;47m";
        m_color_esc_sequence[Color(1)] = "\x1B[1;33;47m";
        m_color_esc_sequence[Color(2)] = "\x1B[1;31;47m";
        m_color_esc_sequence[Color(3)] = "\x1B[1;32;47m";
        m_color_esc_sequence_text[Color(0)] = "\x1B[1;34m";
        m_color_esc_sequence_text[Color(1)] = "\x1B[1;33m";
        m_color_esc_sequence_text[Color(2)] = "\x1B[1;31m";
        m_color_esc_sequence_text[Color(3)] = "\x1B[1;32m";
    }
    BoardType board_type;
    if (game_variant == game_variant_classic
        || game_variant == game_variant_classic_2)
    {
        board_type = board_type_classic;
        m_nu_colors = 4;
    }
    else if (game_variant == game_variant_duo)
    {
        board_type = board_type_duo;
        m_nu_colors = 2;
    }
    else
    {
        LIBBOARDGAME_ASSERT(game_variant == game_variant_trigon
                            || game_variant == game_variant_trigon_2);
        board_type = board_type_trigon;
        m_nu_colors = 4;
    }
    m_board_const = &BoardConst::get(board_type);
    m_geometry = &m_board_const->get_geometry();
    m_starting_points.init(game_variant, *m_geometry);
    m_point_state.init(*m_geometry);
    m_point_state.fill_all(PointStateExt::offboard());
    m_point_state.fill_onboard(PointState::empty());
    m_played_move.init(*m_geometry);
    m_played_move.fill(Move::null());
    for (ColorIterator i(m_nu_colors); i; ++i)
    {
        if (game_variant == game_variant_classic_2
            || game_variant == game_variant_trigon_2)
            m_second_color[*i] =
                (*i).get_next(m_nu_colors).get_next(m_nu_colors);
        else
            m_second_color[*i] = *i;
        m_forbidden[*i].init(*m_geometry);
        m_forbidden[*i].fill_all(true);
        m_forbidden[*i].fill_onboard(false);
        m_is_attach_point[*i].init(*m_geometry, false);
        m_pieces_left[*i].clear();
        for (unsigned int j = 0; j < get_nu_pieces(); ++j)
            m_pieces_left[*i].push_back(j);
    }
    m_to_play = Color(0);
    m_moves.clear();
}

bool Board::is_game_over() const
{
    for (ColorIterator i(m_nu_colors); i; ++i)
        if (has_moves(*i))
            return false;
    return true;
}

bool Board::is_legal(Color c, Move mv) const
{
    if (mv.is_pass())
        return true;
    const MovePoints& points = get_move_points(mv);
    bool has_attach_point = false;
    BOOST_FOREACH(Point p, points)
    {
        if (m_forbidden[c][p])
            return false;
        if (is_attach_point(p, c))
            has_attach_point = true;
    }
    if (has_attach_point)
        return true;
    bool is_first_move = (m_pieces_left[c].size() == get_nu_pieces());
    if (! is_first_move)
        return false;
    BOOST_FOREACH(Point p, get_starting_points(c))
        if (points.contains(p))
            return true;
    return false;
}

bool Board::is_piece_left(Color c, const Piece& piece) const
{
    BOOST_FOREACH(unsigned int i, m_pieces_left[c])
        if (&get_piece(i) == &piece)
            return true;
    return false;
}

void Board::play(Color c, Move mv)
{
    LIBBOARDGAME_ASSERT(! mv.is_null());
    if (! mv.is_pass())
    {
        const MoveInfo& info = m_board_const->get_move_info(mv);
        LIBBOARDGAME_ASSERT(m_pieces_left[c].contains(info.piece));
        m_pieces_left[c].remove(info.piece);
        for (auto i = info.points.begin(); i != info.points.end(); ++i)
        {
            m_point_state[*i] = c;
            m_played_move[*i] = mv;
            for (ColorIterator j(m_nu_colors); j; ++j)
                m_forbidden[*j][*i] = true;
        }
        for (auto i = info.adj_points.begin(); i != info.adj_points.end(); ++i)
            m_forbidden[c][*i] = true;
        for (auto i = info.attach_points.begin(); i != info.attach_points.end();
             ++i)
            m_is_attach_point[c][*i] = true;
    }
    m_moves.push_back(ColorMove(c, mv));
    m_to_play = c.get_next(m_nu_colors);
}

string Board::to_string(Move mv, bool only_points) const
{
    if (mv.is_null())
        return "NULL";
    if (mv.is_pass())
        return "pass";
    const MoveInfo& info = get_move_info(mv);
    ostringstream s;
    if (! only_points)
        s << '[' << get_piece(info.piece).get_name() << "] ";
    s << info.points;
    return s.str();
}

void Board::undo()
{
    LIBBOARDGAME_ASSERT(get_nu_moves() > 0);
    ArrayList<ColorMove, max_game_moves> moves = m_moves;
    moves.pop_back();
    init();
    for (unsigned int i = 0; i < moves.size(); ++i)
        play(moves[i]);
}

void Board::write(ostream& out, bool mark_last_move) const
{
    ColorMove last_mv = ColorMove::null();
    unsigned int n = get_nu_moves();
    while (n > 0)
    {
        --n;
        last_mv = get_move(n);
        if (! last_mv.move.is_pass())
            break;
    }
    unsigned int width = m_geometry->get_width();
    unsigned int height = m_geometry->get_height();
    bool is_info_location_right = (width <= 20);
    bool is_trigon = (get_board_type() == board_type_trigon);
    bool last_mv_marked = false;
    write_x_coord(out, width, is_trigon ? 3 : 2);
    for (unsigned int y = height - 1; ; --y)
    {
        if (y < 9)
            out << ' ';
        out << (y + 1) << ' ';
        for (unsigned int x = 0; x < width; ++x)
        {
            Point p(x, y);
            PointStateExt s = get_point_state_ext(p);
            if ((x > 0
                 || (is_trigon
                     && ! get_point_state_ext(Point(x + 1, y)).is_offboard()))
                && ! s.is_offboard())
            {
                if (mark_last_move && ! last_mv_marked && ! last_mv.is_null()
                    && get_point_state_ext(p.get_left()) != last_mv.color
                    && get_played_move(Point(x, y)) == last_mv.move)
                {
                    set_color(out, "\x1B[1;37;47m");
                    out << '>';
                    last_mv_marked = true;
                }
                else if (mark_last_move && ! last_mv_marked
                         && ! last_mv.is_null() && s != last_mv.color
                         && get_point_state_ext(p.get_left()) == last_mv.color
                         && get_played_move(p.get_left()) == last_mv.move)
                {
                    set_color(out, "\x1B[1;37;47m");
                    out << '<';
                    last_mv_marked = true;
                }
                else if (is_trigon)
                {
                    set_color(out, "\x1B[1;30;47m");
                    out << (x % 2 == y % 2 ? '\\' : '/');
                }
                else
                {
                    set_color(out, "\x1B[1;30;47m");
                    out << ' ';
                }
            }
            if (s.is_offboard())
            {
                if (is_trigon && x > 0
                    && ! get_point_state_ext(p.get_left()).is_offboard())
                {
                    set_color(out, "\x1B[1;30;47m");
                    out << (x % 2 == y % 2 ? '\\' : '/');
                }
                else
                {
                    set_color(out, "\x1B[0m");
                    out << "  ";
                }
            }
            else if (s.is_empty())
            {
                if (is_colored_starting_point(p))
                {
                    Color c = get_starting_point_color(p);
                    set_color(out, m_color_esc_sequence[c]);
                    out << '+';
                }
                else if (is_colorless_starting_point(p))
                {
                    set_color(out, "\x1B[1;30;47m");
                    out << '+';
                }
                else
                {
                    set_color(out, "\x1B[1;30;47m");
                    out << (is_trigon ? ' ' : '.');
                }
            }
            else
            {
                Color color = s.to_color();
                set_color(out, m_color_esc_sequence[color]);
                out << m_color_char[color];
            }
        }
        if (is_trigon)
        {
            if (! get_point_state_ext(Point(width - 1, y)).is_offboard())
            {
                set_color(out, "\x1B[1;30;47m");
                out << (y % 2 != 0 ? '\\' : '/');
            }
            else
            {
                set_color(out, "\x1B[0m");
                out << "  ";
            }
        }
        set_color(out, "\x1B[0m");
        out << ' ' << (y + 1);
        if (is_info_location_right)
        {
            if (y < 9)
                out << "   ";
            else
                out << "  ";
            write_info_line(out, height - y - 1);
        }
        out << '\n';
        if (y == 0)
            break;
    }
    write_x_coord(out, width, is_trigon ? 3 : 2);
    if (! is_info_location_right)
        for (ColorIterator i(m_nu_colors); i; ++i)
        {
            write_color_info_line1(out, *i);
            out << ' ';
            write_color_info_line2(out, *i);
            out << ' ';
            write_color_info_line3(out, *i);
            out << '\n';
        }
}

void Board::write_color_info_line1(ostream& out, Color c) const
{
    set_color(out, m_color_esc_sequence_text[c]);
    out << m_color_name[c] << "(" << m_color_char[c] << "): " << get_points(c);
    unsigned int bonus = get_bonus(c);
    if (bonus > 0)
        out << " (+" << bonus << ')';
    if (get_to_play() == c)
        out << " *";
    set_color(out, "\x1B[0m");
}

void Board::write_color_info_line2(ostream& out, Color c) const
{
    write_pieces_left(out, c, 0, 10);
}

void Board::write_color_info_line3(ostream& out, Color c) const
{
    write_pieces_left(out, c, 10, get_nu_pieces());
}

void Board::write_info_line(ostream& out, unsigned int y) const
{
    if (y == 0)
        write_color_info_line1(out, Color(0));
    else if (y == 1)
        write_color_info_line2(out, Color(0));
    else if (y == 2)
        write_color_info_line3(out, Color(0));
    else if (y == 4)
        write_color_info_line1(out, Color(1));
    else if (y == 5)
        write_color_info_line2(out, Color(1));
    else if (y == 6)
        write_color_info_line3(out, Color(1));
    else if (y == 8 && m_nu_colors > 2)
        write_color_info_line1(out, Color(2));
    else if (y == 9 && m_nu_colors > 2)
        write_color_info_line2(out, Color(2));
    else if (y == 10 && m_nu_colors > 2)
        write_color_info_line3(out, Color(2));
    else if (y == 12 && m_nu_colors > 3)
        write_color_info_line1(out, Color(3));
    else if (y == 13 && m_nu_colors > 3)
        write_color_info_line2(out, Color(3));
    else if (y == 14 && m_nu_colors > 3)
        write_color_info_line3(out, Color(3));
}

void Board::write_pieces_left(ostream& out, Color c, unsigned int begin,
                              unsigned int end) const
{
    for (unsigned int i = begin; i < end; ++i)
        if (i < m_pieces_left[c].size())
        {
            if (i > begin)
                out << ' ';
            out << get_piece(m_pieces_left[c][i]).get_name();
        }
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_base
