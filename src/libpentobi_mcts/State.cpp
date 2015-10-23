//-----------------------------------------------------------------------------
/** @file libpentobi_mcts/State.cpp
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "State.h"

#include "libboardgame_util/MathUtil.h"
#include "libpentobi_base/ScoreUtil.h"
#if LIBBOARDGAME_DEBUG
#include "libpentobi_base/BoardUtil.h"
#endif

namespace libpentobi_mcts {

using libboardgame_util::fast_exp;
using libpentobi_base::get_multiplayer_result;
using libpentobi_base::BoardType;
using libpentobi_base::PointState;

//-----------------------------------------------------------------------------

namespace {

inline Float sigmoid(Float steepness, Float x)
{
    return -1.f + 2.f / (1.f + fast_exp(-steepness * x));
}

} // namespace

//-----------------------------------------------------------------------------

State::State(Variant initial_variant, const SharedConst& shared_const)
  : m_shared_const(shared_const),
    m_bd(initial_variant)
{
}

inline void State::add_moves(Point p, Color c,
                             const Board::PiecesLeftList& pieces_considered)
{
    auto& moves = m_moves[c];
    auto nu_moves = moves.size();
    auto& marker = m_marker[c];
    auto& playout_features = m_playout_features[c];
    auto adj_status = m_bd.get_adj_status(p, c);
    double total_gamma =
            moves.empty() ? 0 : m_cumulative_gamma[moves.size() - 1];
    for (Piece piece : pieces_considered)
        for (Move mv : get_moves(c, piece, p, adj_status))
            if (! marker[mv]
                    && check_move(mv, get_move_info(mv), moves, nu_moves,
                                  playout_features, total_gamma))
                marker.set(mv);
    moves.resize(nu_moves);
    m_moves_added_at[c][p] = true;
}

inline void State::add_moves(Point p, Color c, Piece piece,
                             unsigned adj_status, double& total_gamma)
{
    auto& moves = m_moves[c];
    auto nu_moves = moves.size();
    auto& marker = m_marker[c];
    auto& playout_features = m_playout_features[c];
    for (Move mv : get_moves(c, piece, p, adj_status))
        if (! marker[mv]
                && check_move(mv, get_move_info(mv), moves, nu_moves,
                              playout_features, total_gamma))
            marker.set(mv);
    moves.resize(nu_moves);
}

void State::add_starting_moves(Color c,
                               const Board::PiecesLeftList& pieces_considered,
                               bool with_gamma)
{
    // Using only one starting point (if game variant has more than one) not
    // only reduces the branching factor but is also necessary because
    // update_moves() assumes that a move stays legal if the forbidden
    // status for all of its points does not change.
    Point p = find_best_starting_point(c);
    if (p.is_null())
        return;
    auto& moves = m_moves[c];
    LIBBOARDGAME_ASSERT(moves.empty());
    unsigned nu_moves = 0;
    auto& marker = m_marker[c];
    auto& is_forbidden = m_bd.is_forbidden(c);
    double total_gamma = 0;
    for (Piece piece : pieces_considered)
        for (Move mv : get_moves(c, piece, p, 0))
        {
            LIBBOARDGAME_ASSERT(! marker[mv]);
            if (check_forbidden(is_forbidden, mv, moves, nu_moves))
            {
                marker.set(mv);
                if (with_gamma)
                {
                    total_gamma += m_gamma_piece[piece];
                    m_cumulative_gamma[nu_moves - 1] = total_gamma;
                }
            }
        }
    moves.resize(nu_moves);
}

bool State::check_forbidden(const Grid<bool>& is_forbidden, Move mv,
                            MoveList& moves, unsigned& nu_moves)
{
    auto& info = get_move_info(mv);
    auto p = info.begin();
    if (is_forbidden[*p])
        return false;
    auto end = info.end();
    // Loop over fixed size to help compiler unroll
    for (unsigned i = 1; i < PieceInfo::max_size; ++i)
    {
        if (++p == end)
            break;
        if (is_forbidden[*p])
            return false;
    }
    LIBBOARDGAME_ASSERT(nu_moves < MoveList::max_size);
    moves.get_unchecked(nu_moves) = mv;
    ++nu_moves;
    return true;
}

bool State::check_move(Move mv, const MoveInfo& info, MoveList& moves,
                       unsigned& nu_moves,
                       const PlayoutFeatures& playout_features,
                       double& total_gamma)
{
    auto p = info.begin();
    PlayoutFeatures::Compute features(*p, playout_features);
    if (features.is_forbidden())
        return false;
    auto end = info.end();
    // Loop over fixed size to help compiler unroll
    for (unsigned i = 1; i < PieceInfo::max_size; ++i)
    {
        if (++p == end)
            break;
        if (! features.add(*p, playout_features))
            return false;
    }
    double gamma = m_gamma_piece[info.get_piece()];
    auto nu_local = features.get_nu_local();
    if (nu_local > 0)
        gamma *= m_gamma_local[nu_local];
    total_gamma += gamma;
    m_cumulative_gamma[nu_moves] = total_gamma;
    LIBBOARDGAME_ASSERT(nu_moves < MoveList::max_size);
    moves.get_unchecked(nu_moves) = mv;
    ++nu_moves;
    return true;
}

#if LIBBOARDGAME_DEBUG
string State::dump() const
{
    ostringstream s;
    s << "pentobi_mcts::State:\n" << libpentobi_base::boardutil::dump(m_bd);
    return s.str();
}
#endif

/** Evaluation function for Duo and Junior. */
void State::evaluate_duo(array<Float, 6>& result)
{
    LIBBOARDGAME_ASSERT(m_bd.get_nu_players() == 2);
    LIBBOARDGAME_ASSERT(m_bd.get_nu_colors() == 2);
    if (! m_is_symmetry_broken
            && m_bd.get_nu_onboard_pieces() >= m_symmetry_min_nu_pieces)
    {
        if (log_simulations)
            log("Result: 0.5 (symmetry)");
        result[0] = result[1] = 0.5;
        return;
    }
    Color c(0);
    auto s = static_cast<Float>(m_bd.get_score(c));
    Float res;
    if (s > 0)
        res = 1;
    else if (s < 0)
        res = 0;
    else
        res = 0.5;
    if (log_simulations)
        log("Result color ", c, ": sco=", s, " game_res=", res);
    res += get_quality_bonus(c, res, s, false);
    if (log_simulations)
        log("res=", res);
    result[0] = res;
    result[1] = 1.f - res;
}

/** Evaluation function for game variants with 2 players and 2 colors per
    player. */
void State::evaluate_multicolor(array<Float, 6>& result)
{
    LIBBOARDGAME_ASSERT(m_bd.get_nu_players() == 2);
    LIBBOARDGAME_ASSERT(m_bd.get_nu_colors() == 4);
    // Always evaluate symmetric positions in trigon_2 as a draw in the
    // playouts. See comment in evaluate_playout_duo.
    // m_is_symmetry_broken is always true in classic_2, no need to check for
    // game variant.
    if (! m_is_symmetry_broken
            && m_bd.get_nu_onboard_pieces() >= m_symmetry_min_nu_pieces)
    {
        if (log_simulations)
            log("Result: 0.5 (symmetry)");
        result[0] = result[1] = result[2] = result[3] = 0.5;
        return;
    }

    Color c(0);
    auto s = static_cast<Float>(m_bd.get_score(c));
    Float res;
    if (s > 0)
        res = 1;
    else if (s < 0)
        res = 0;
    else
        res = 0.5;
    if (log_simulations)
        log("Result color ", c, ": sco=", s, " game_res=", res);
    res += get_quality_bonus(c, res, s, true);
    if (log_simulations)
        log("res=", res);
    result[0] = result[2] = res;
    result[1] = result[3] = 1.f - res;
}

/** Evaluation function for game variants with more than 2 players.
    The result is 0,0.5,1 for loss/tie/win in 2-player variants. For n \> 2
    players, this is generalized in the following way: The scores are sorted in
    ascending order. Each rank r_i (i in 0..n-1) is assigned a result value of
    r_i/(n-1). If multiple players have the same score, the result value is the
    average of all ranks with this score. So being the single winner still
    gives the result 1 and having the lowest score gives the result 0. Being
    the single winner is better than sharing the best place, which is better
    than getting the second place, etc. */
void State::evaluate_multiplayer(array<Float, 6>& result)
{
    auto nu_players = m_bd.get_nu_players();
    LIBBOARDGAME_ASSERT(nu_players > 2);
    array<unsigned, Color::range> points;
    for (Color::IntType i = 0; i < nu_players; ++i)
        points[i] = m_bd.get_points(Color(i));
    array<Float, Color::range> game_result;
    get_multiplayer_result(nu_players, points, game_result);
    for (Color::IntType i = 0; i < nu_players; ++i)
    {
        Color c(i);
        auto s = static_cast<Float>(m_bd.get_score(c));
        result[i] =
                game_result[i] + get_quality_bonus(c, game_result[i], s, true);
        if (log_simulations)
            log("Result color ", c, ": sco=", s, " game_res=", game_result[i],
                " res=", result[i]);
    }
    if (m_bd.get_variant() == Variant::classic_3)
    {
        result[3] = result[0];
        result[4] = result[1];
        result[5] = result[2];
    }
}

Point State::find_best_starting_point(Color c) const
{
    // We use the starting point that maximizes the distance to occupied
    // starting points, especially to the ones occupied by the player (their
    // distance is weighted with a factor of 2)
    Point best = Point::null();
    float max_distance = -1;
    bool is_trigon = (m_bd.get_board_type() == BoardType::trigon
                      || m_bd.get_board_type() == BoardType::trigon_3);
    float ratio = (is_trigon ? 1.732f : 1);
    auto& geo = m_bd.get_geometry();
    for (Point p : m_bd.get_starting_points(c))
    {
        if (m_bd.is_forbidden(p, c))
            continue;
        float px = static_cast<float>(geo.get_x(p));
        float py = static_cast<float>(geo.get_y(p));
        float d = 0;
        for (Color i : Color::Range(m_nu_colors))
            for (Point pp : m_bd.get_starting_points(i))
            {
                PointState s = m_bd.get_point_state(pp);
                if (! s.is_empty())
                {
                    float ppx = static_cast<float>(geo.get_x(pp));
                    float ppy = static_cast<float>(geo.get_y(pp));
                    float dx = ppx - px;
                    float dy = ratio * (ppy - py);
                    float weight = 1;
                    if (s == c || s == m_bd.get_second_color(c))
                        weight = 2;
                    d += weight * sqrt(dx * dx + dy * dy);
                }
            }
        if (d > max_distance)
        {
            best = p;
            max_distance = d;
        }
    }
    return best;
}

bool State::gen_playout_move_full(PlayerMove<Move>& mv)
{
    Color to_play = m_bd.get_to_play();
    while (true)
    {
        if (! m_is_move_list_initialized[to_play])
            init_moves_with_gamma(to_play);
        else if (m_has_moves[to_play])
            update_moves(to_play);
        if ((m_has_moves[to_play] = ! m_moves[to_play].empty()))
            break;
        if (++m_nu_passes == m_nu_colors)
            return false;
        if (m_check_terminate_early && m_bd.get_score(to_play) < 0
            && ! m_has_moves[m_bd.get_second_color(to_play)])
        {
            if (log_simulations)
                log("Terminate early (no moves and negative score)");
            return false;
        }
        to_play = to_play.get_next(m_nu_colors);
        m_bd.set_to_play(to_play);
        // Don't try to handle symmetry after pass moves
        m_is_symmetry_broken = true;
    }

    auto& moves = m_moves[to_play];
    double total_gamma = m_cumulative_gamma[moves.size() - 1];
    if (log_simulations)
        log("Moves: ", moves.size(), ", total_gamma: ", total_gamma);
    auto begin = m_cumulative_gamma.begin();
    auto end = begin + moves.size();
    auto random = m_random.generate_double(0, total_gamma);
    auto pos = lower_bound(begin, end, random);
    LIBBOARDGAME_ASSERT(pos != end);
    mv = PlayerMove<Move>(get_player(),
                          moves[static_cast<unsigned>(pos - begin)]);
    return true;
}

inline const PieceMap<bool>& State::get_pieces_considered() const
{
    // Use number of on-board pieces for move number to handle the case where
    // there are more pieces on the board than moves (setup positions)
    unsigned nu_moves = m_bd.get_nu_onboard_pieces();
    if (nu_moves >= m_shared_const.min_move_all_considered
        || m_force_consider_all_pieces)
        return m_shared_const.is_piece_considered_all;
    else
        return *m_shared_const.is_piece_considered[nu_moves];
}

/** Bonus added to the result for quality-based rewards.
    See also: Pepels et al.: Quality-based Rewards for Monte-Carlo Tree Search
    Simulations. ECAI 2014. */
inline Float State::get_quality_bonus(Color c, Float result, Float score,
                                      bool use_nu_attach)
{
    Float bonus = 0;

    // Game length
    Float l = static_cast<Float>(m_bd.get_nu_moves());
    m_stat_len.add(l);
    Float var = m_stat_len.get_variance();
    if (var > 0)
        bonus +=
                (result == 1 ? -0.06f : 0.06f)
                * sigmoid(2.f, (l - m_stat_len.get_mean()) / sqrt(var));

    // Game score
    auto& stat = m_stat_score[c];
    stat.add(score);
    var = stat.get_variance();
    if (var > 0)
        bonus += 0.3f * sigmoid(2.f, (score - stat.get_mean()) / sqrt(var));

    // Number of non-forbidden attach points is another feature of a superior
    // final position. Not used in Duo/Junior, mainly helps in Trigon.
    if (! use_nu_attach)
        return bonus;
    auto second_color = m_bd.get_second_color(c);
    Float opp_weight = (m_bd.get_nu_players() == 2 ? 1.f : 1.f / m_nu_colors);
    Float attach = 0;
    for (Color i : Color::Range(m_nu_colors))
        if (i == c || i == second_color)
        {
            for (Point p : m_bd.get_attach_points(i))
                if (! m_bd.is_forbidden(p, i))
                    ++attach;
        }
        else
        {
            Float n = 0;
            for (Point p : m_bd.get_attach_points(i))
                if (! m_bd.is_forbidden(p, i))
                    ++n;
            attach -= opp_weight * n;
        }
    m_stat_attach.add(attach);
    var = m_stat_attach.get_variance();
    if (var > 0)
        bonus +=
          0.1f * sigmoid(2.f, (attach - m_stat_attach.get_mean()) / sqrt(var));
    return bonus;
}

void State::init_moves_with_gamma(Color c)
{
    m_is_piece_considered[c] = &get_pieces_considered();
    auto& is_piece_considered = *m_is_piece_considered[c];
    m_playout_features[c].set_local(m_bd);
    auto& marker = m_marker[c];
    auto& moves = m_moves[c];
    marker.clear(moves);
    moves.clear();
    Board::PiecesLeftList pieces_considered;
    for (Piece piece : m_bd.get_pieces_left(c))
        if ((*m_is_piece_considered[c])[piece])
            pieces_considered.push_back(piece);
    if (m_bd.is_first_piece(c))
        add_starting_moves(c, pieces_considered, true);
    else
        for (Point p : m_bd.get_attach_points(c))
            if (! m_bd.is_forbidden(p, c))
                add_moves(p, c, pieces_considered);
    m_is_move_list_initialized[c] = true;
    m_nu_new_moves[c] = 0;
    m_last_attach_points_end[c] = m_bd.get_attach_points(c).end();
    if (moves.empty()
            && &is_piece_considered != &m_shared_const.is_piece_considered_all)
    {
        m_force_consider_all_pieces = true;
        init_moves_with_gamma(c);
    }
}

void State::init_moves_without_gamma(Color c)
{
    m_is_piece_considered[c] = &get_pieces_considered();
    auto& is_piece_considered = *m_is_piece_considered[c];
    auto& marker = m_marker[c];
    auto& moves = m_moves[c];
    marker.clear(moves);
    moves.clear();
    Board::PiecesLeftList pieces_considered;
    for (Piece piece : m_bd.get_pieces_left(c))
        if (is_piece_considered[piece])
            pieces_considered.push_back(piece);
    auto& is_forbidden = m_bd.is_forbidden(c);
    if (m_bd.is_first_piece(c))
        add_starting_moves(c, pieces_considered, false);
    else
    {
        unsigned nu_moves = 0;
        for (Point p : m_bd.get_attach_points(c))
            if (! is_forbidden[p])
            {
                auto adj_status = m_bd.get_adj_status(p, c);
                for (Piece piece : pieces_considered)
                    for (Move mv : get_moves(c, piece, p, adj_status))
                        if (! marker[mv]
                                && check_forbidden(is_forbidden, mv, moves,
                                                   nu_moves))
                            marker.set(mv);
                m_moves_added_at[c][p] = true;
            }
        moves.resize(nu_moves);
    }
    m_is_move_list_initialized[c] = true;
    m_nu_new_moves[c] = 0;
    m_last_attach_points_end[c] = m_bd.get_attach_points(c).end();
    if (moves.empty()
            && &is_piece_considered != &m_shared_const.is_piece_considered_all)
    {
        m_force_consider_all_pieces = true;
        init_moves_without_gamma(c);
    }
}

void State::play_expanded_child(Move mv)
{
    if (log_simulations)
        log("Playing expanded child");
    if (! mv.is_null())
        play_playout(mv);
    else
    {
        ++m_nu_passes;
        m_bd.set_to_play(m_bd.get_to_play().get_next(m_nu_colors));
        // Don't try to handle pass moves: a pass move either breaks symmetry
        // or both players have passed and it's the end of the game and we need
        // symmetry detection only as a heuristic (playouts and move value
        // initialization)
        m_is_symmetry_broken = true;
        if (log_simulations)
            log(m_bd);
    }
}

void State::start_search()
{
    auto& bd = *m_shared_const.board;
    m_bd.copy_from(bd);
    m_bd.set_to_play(m_shared_const.to_play);
    m_bd.take_snapshot();
    m_nu_colors = bd.get_nu_colors();
    for (Color c : Color::Range(m_nu_colors))
        m_playout_features[c].init_snapshot(m_bd, c);
    m_bc = &m_bd.get_board_const();
    m_move_info_array = m_bc->get_move_info_array();
    m_move_info_ext_array = m_bc->get_move_info_ext_array();
    m_check_terminate_early =
            (bd.get_nu_moves() < 10u * m_nu_colors
             && m_bd.get_nu_players() == 2);
    auto variant = bd.get_variant();
    m_check_symmetric_draw =
        ((variant == Variant::duo || variant == Variant::junior
          || variant == Variant::trigon_2)
         && ! ((m_shared_const.to_play == Color(1)
                || m_shared_const.to_play == Color(3))
               && m_shared_const.avoid_symmetric_draw)
         && ! check_symmetry_broken(bd, m_shared_const.symmetric_points));
    if (! m_check_symmetric_draw)
        // Pretending that the symmetry is always broken is equivalent to
        // ignoring symmetric draws
        m_is_symmetry_broken = true;
    if (variant == Variant::trigon_2)
        m_symmetry_min_nu_pieces = 5;
    else
        m_symmetry_min_nu_pieces = 3; // Only used in Duo

    m_prior_knowledge.start_search(bd);
    m_stat_len.clear();
    m_stat_attach.clear();
    for (Color c : Color::Range(m_nu_colors))
        m_stat_score[c].clear();

    // Init gamma values
    double gamma_size_factor = 1;
    double gamma_nu_attach_factor = 1;
    switch (bd.get_board_type())
    {
    case BoardType::classic:
        gamma_size_factor = 5;
        break;
    case BoardType::duo:
        gamma_size_factor = 3;
        gamma_nu_attach_factor = 1.8;
        break;
    case BoardType::trigon:
    case BoardType::trigon_3:
        gamma_size_factor = 5;
        break;
    }
    for (Piece::IntType i = 0; i < m_bc->get_nu_pieces(); ++i)
    {
        Piece piece(i);
        auto piece_size = m_bc->get_piece_info(piece).get_size();
        LIBBOARDGAME_ASSERT(piece_size > 0);
        auto piece_nu_attach = m_bc->get_nu_attach_points(piece);
        LIBBOARDGAME_ASSERT(piece_nu_attach > 0);
        m_gamma_piece[piece] =
            pow(gamma_size_factor, piece_size - 1)
            * pow(gamma_nu_attach_factor, piece_nu_attach - 1);
    }
    // The value of nu_local dominates all other features, so we use a high
    // gamma. Above some limit, we don't care about the exact value.
    double gamma = 1;
    for (unsigned i = 0; i < PlayoutFeatures::max_local + 1; ++i)
    {
        m_gamma_local[i] = gamma;
        if (i < 5)
            gamma *= 1e6;
    }
}

void State::start_simulation(size_t n)
{
    if (log_simulations)
        log("=========================================================\n",
            "Simulation ", n, "\n",
            "=========================================================");
    m_bd.restore_snapshot();
    m_force_consider_all_pieces = false;
    auto& geo = m_bd.get_geometry();
    for (Color c : Color::Range(m_nu_colors))
    {
        m_has_moves[c] = true;
        m_is_move_list_initialized[c] = false;
        m_playout_features[c].restore_snapshot(m_bd);
        m_moves_added_at[c].fill(false, geo);
    }
    m_nu_passes = 0;
}

void State::update_moves(Color c)
{
    auto& playout_features = m_playout_features[c];
    playout_features.set_local(m_bd);

    auto& marker = m_marker[c];

    // Find old moves that are still legal
    auto& is_forbidden = m_bd.is_forbidden(c);
    auto& moves = m_moves[c];
    auto old_size = moves.size();
    moves.clear();
    unsigned nu_moves = 0;
    double total_gamma = 0;
    if (m_nu_new_moves[c] == 1 && m_bd.get_nu_piece_instances() == 1)
    {
        Piece piece = get_move_info(m_last_move[c]).get_piece();
        for (unsigned i = 0; i < old_size; ++i)
        {
            LIBBOARDGAME_ASSERT(i >= moves.size());
            Move mv = moves.get_unchecked(i);
            auto& info = get_move_info(mv);
            if (info.get_piece() == piece
                    || ! check_move(mv, info, moves, nu_moves,
                                    playout_features, total_gamma))
                marker.clear(mv);
        }
    }
    else
    {
        PieceMap<bool> is_piece_left(false);
        for (Piece piece : m_bd.get_pieces_left(c))
            is_piece_left[piece] = true;
        for (unsigned i = 0; i < old_size; ++i)
        {
            LIBBOARDGAME_ASSERT(i >= moves.size());
            Move mv = moves.get_unchecked(i);
            auto& info = get_move_info(mv);
            if (! is_piece_left[info.get_piece()]
                    || ! check_move(mv, info, moves,  nu_moves,
                                    playout_features, total_gamma))
                marker.clear(mv);
        }
    }
    moves.resize(nu_moves);

    // Find new legal moves because of new pieces played by this color
    Board::PiecesLeftList pieces_considered;
    for (Piece piece : m_bd.get_pieces_left(c))
        if ((*m_is_piece_considered[c])[piece])
            pieces_considered.push_back(piece);
    auto& attach_points = m_bd.get_attach_points(c);
    auto begin = m_last_attach_points_end[c];
    auto end = attach_points.end();
    for (auto i = begin; i != end; ++i)
        if (! is_forbidden[*i] && ! m_moves_added_at[c][*i])
            add_moves(*i, c, pieces_considered);
    m_nu_new_moves[c] = 0;
    m_last_attach_points_end[c] = end;

    // Generate moves for pieces not considered in the last position
    auto& is_piece_considered = *m_is_piece_considered[c];
    if (&is_piece_considered != &m_shared_const.is_piece_considered_all)
    {
        if (moves.empty())
            m_force_consider_all_pieces = true;
        auto& is_piece_considered_new = get_pieces_considered();
        if (&is_piece_considered !=  &is_piece_considered_new)
        {
            pieces_considered.clear();
            for (Piece piece : m_bd.get_pieces_left(c))
                if (! is_piece_considered[piece]
                    && is_piece_considered_new[piece])
                    pieces_considered.push_back(piece);
            double total_gamma =
                    moves.empty() ? 0 : m_cumulative_gamma[moves.size() - 1];
            for (Point p : m_bd.get_attach_points(c))
                if (! is_forbidden[p])
                {
                    auto adj_status = m_bd.get_adj_status(p, c);
                    for (Piece piece : pieces_considered)
                        add_moves(p, c, piece, adj_status, total_gamma);
                }
            m_is_piece_considered[c] = &is_piece_considered_new;
        }
    }
}

void State::update_symmetry_broken(Move mv)
{
    Color to_play = m_bd.get_to_play();
    Color second_color = m_bd.get_second_color(to_play);
    auto& info = get_move_info(mv);
    auto i = info.begin();
    auto end = info.end();
    if (to_play == Color(0) || to_play == Color(2))
    {
        // First player to play: Check that all symmetric points of the last
        // move of the second player are occupied by the first player
        do
        {
            Point symm_p = m_shared_const.symmetric_points[*i];
            if (m_bd.get_point_state(symm_p) != second_color)
            {
                m_is_symmetry_broken = true;
                return;
            }
        }
        while (++i != end);
    }
    else
    {
        // Second player to play: Check that all symmetric points of the last
        // move of the first player are empty (i.e. the second player can play
        // there to preserve the symmetry)
        do
        {
            Point symm_p = m_shared_const.symmetric_points[*i];
            if (! m_bd.get_point_state(symm_p).is_empty())
            {
                m_is_symmetry_broken = true;
                return;
            }
        }
        while (++i != end);
    }
}

string State::get_info() const
{
    ostringstream s;
    if (m_bd.get_nu_players() == 2)
    {
        s << "Sco: ";
        m_stat_score[Color(0)].write(s, true, 1);
    }
    s << '\n';
    return s.str();
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_mcts
