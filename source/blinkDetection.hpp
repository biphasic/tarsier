#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tarsier {

/// BlinkDetection detects blinks wow
template <typename Event, typename BlinkEvent, uint64_t lifespan,
          uint64_t lowerThreshold, uint64_t upperThreshold,
          typename BlinkEventFromEvent, typename HandleBlinkEvent>
class BlinkDetection {
public:
  BlinkDetection(BlinkEventFromEvent blinkEventFromEvent,
                 HandleBlinkEvent handleBlinkDetectionEvent)
      : _blinkEventFromEvent(
            std::forward<BlinkEventFromEvent>(blinkEventFromEvent)),
        _handleBlinkEvent(
            std::forward<HandleBlinkEvent>(handleBlinkDetectionEvent)),
        _lifespan(lifespan), _lowerThreshold(lowerThreshold),
        _upperThreshold(upperThreshold), _lastTimeStamp(0), _xGridSize(19),
        _yGridSize(20), _isBlink(false), _x2(0), _y2(0) {}

  BlinkDetection(const BlinkDetection &) = delete;
  BlinkDetection(BlinkDetection &&) = default;
  BlinkDetection &operator=(const BlinkDetection &) = delete;
  BlinkDetection &operator=(BlinkDetection &&) = default;
  virtual ~BlinkDetection() {}

  virtual void operator()(Event event) {
    _col = static_cast<int>(event.x / _xGridSize);
    _row = static_cast<int>(event.y / _yGridSize);

    _currentTimeStamp = event.timestamp;
    _lastTimeStamp = _grid[_col][_row].latestTimestamp;

    // updating activities
    for (size_t i = 0; i <= 15; i++) {
      // only update tiles that are beyond the lower threshold. Values below
      // are of no interest to an exponential decay
      if (_grid[i][_row].activity > _lowerThreshold || i == _col) {
        _grid[i][_row].activity *=
            exp(-static_cast<double>(_currentTimeStamp -
                                     _grid[i][_row].latestTimestamp) /
                _lifespan);
        _grid[i][_row].latestTimestamp = _currentTimeStamp;
        if (i == _col) {
          // increase activity only in currently focused tile
          _grid[i][_row].activity += 1;
        }

        // state machine
        switch (_grid[i][_row].state) {
        // too little activity
        case State::background:
          // see if activity has risen above lowerThreshold to set blink
          // candidate, state to 2
          if (_grid[i][_row].activity > _lowerThreshold) {
            _grid[i][_row].state = State::candidate;
            _grid[i][_row].blinkBeginTimestamp = _currentTimeStamp;
            std::cout << i << "/" << _row << "+" << '\n';
          } else if (_grid[i][_row].activity > _upperThreshold) {
            throw std::logic_error("Skipped a state");
          }
          break;
        // within thresholds, possible blink
        case State::candidate:
          // check whether activity may dropped below threshold plus some
          // safety margin
          if (_grid[i][_row].activity < (_lowerThreshold - 10)) {
            std::cout << i << "/" << _row << "-" << '\n';
            _grid[i][_row].state = State::background;
            _blinkCandidateVector[_row].push_back(std::make_pair(
                i, static_cast<long>((_grid[i][_row].latestTimestamp -
                                      _grid[i][_row].blinkBeginTimestamp) /
                                     2) +
                       _grid[i][_row].blinkBeginTimestamp));
          } else if (_grid[i][_row].activity > _upperThreshold) {
            _grid[i][_row].state = State::clutter; // considered clutter now
            std::cout << i << "/" << _row << "[++]" << '\n';
          }
          break;
        // too much activity
        case State::clutter:
          if (_grid[i][_row].activity < _lowerThreshold) {
            _grid[i][_row].state = State::background;
            std::cout << i << "/" << _row << "[--]" << '\n';
          }
          break;
        default:
          // TODO refactor
          _grid[i][_row].state = State::background;
          std::cout << "set state for first time. " << '\n';
          break;
        }
      }
    }

    // check whether central timestamps of the last two blinking candidates
    // (eyes) are not more than 50ms apart and candidates are two tiles apart
    if (_blinkCandidateVector[_row].size() > 1 &&
        abs((_blinkCandidateVector[_row].rbegin() + 1)->second -
            _blinkCandidateVector[_row].back().second) < 50000 &&
        (abs(((_blinkCandidateVector[_row].rbegin() + 1)->first -
              _blinkCandidateVector[_row].back().first)) == 2
         //||
         // abs(((_blinkCandidateVector[_row].rbegin() + 1)->first -
         //_blinkCandidateVector[_row].back().first)) == 1
         )) {

      _blinkVector[_row].push_back(std::make_pair(
          static_cast<int>(((_blinkCandidateVector[_row].rbegin() + 1)->first +
                            _blinkCandidateVector[_row].back().first) /
                           2),
          ((_blinkCandidateVector[_row].rbegin() + 1)->second +
           _blinkCandidateVector[_row].back().second) /
              2));
      if (_blinkVector[_row].size() > 1 &&
          (_blinkVector[_row].back().first -
           (_blinkVector[_row].rbegin() + 1)->first) < 2 &&
          (_blinkVector[_row].back().second -
           (_blinkVector[_row].rbegin() + 1)->second) > 800000 &&
          (_blinkVector[_row].back().second -
           (_blinkVector[_row].rbegin() + 1)->second) < 4000000) {
        _isBlink = true;
        _x2 = ((_blinkCandidateVector[_row].rbegin() + 1)->first) * 19 + 10;
        _y2 = _row * 20 + 10;
        std::cout << "_x2: " << _x2 << " and _y2: " << _y2 << '\n';
        _blinkCandidateVector[_row].clear();
      }
    } else {
      _isBlink = false;
    }

    _handleBlinkEvent(_blinkEventFromEvent(event, _isBlink, _x2, _y2));
  }

protected:
  enum class State {
    background,
    candidate,
    clutter,
  };

  struct Tile {
    long latestTimestamp;
    long blinkBeginTimestamp;
    double activity;
    State state;
  };

  struct BlinkCandidate {
    const short tileIndex;
    const short x;
    const short y;
    const long meanTimestamp;
    const long duration;
  };

  struct Blink {
    BlinkCandidate leftEye;
    BlinkCandidate rightEye;
  };

  BlinkEventFromEvent _blinkEventFromEvent;
  const long _lifespan;
  long _currentTimeStamp;
  long _lastTimeStamp;
  short _xGridSize;
  short _yGridSize;
  short _col;
  short _row;
  std::vector<std::pair<int, long>> _blinkCandidateVector[12];
  std::vector<BlinkCandidate> _blinkCandidates[12];
  std::vector<std::pair<int, long>> _blinkVector[12];
  std::vector<Blink> _blinks[12];
  Tile _grid[16][12];
  bool _isBlink;
  int _x2;
  int _y2;
  double _lowerThreshold;
  double _upperThreshold;
  HandleBlinkEvent _handleBlinkEvent;
};

template <typename Event, typename BlinkEvent, uint64_t lifespan,
          uint64_t lowerThreshold, uint64_t upperThreshold,
          typename BlinkEventFromEvent, typename HandleBlinkEvent>
BlinkDetection<Event, BlinkEvent, lifespan, lowerThreshold, upperThreshold,
               BlinkEventFromEvent, HandleBlinkEvent>
make_blinkDetection(BlinkEventFromEvent blinkEventFromEvent,
                    HandleBlinkEvent handleBlinkDetectionEvent) {
  return BlinkDetection<Event, BlinkEvent, lifespan, lowerThreshold,
                        upperThreshold, BlinkEventFromEvent, HandleBlinkEvent>(
      std::forward<BlinkEventFromEvent>(blinkEventFromEvent),
      std::forward<HandleBlinkEvent>(handleBlinkDetectionEvent));
}
}
