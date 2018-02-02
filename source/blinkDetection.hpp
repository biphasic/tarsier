#pragma once

#include <algorithm>
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
        _upperThreshold(upperThreshold), _xGridSize(19), _yGridSize(20),
        _isBlink(false), _x2(0), _y2(0) {}

  BlinkDetection(const BlinkDetection &) = delete;
  BlinkDetection(BlinkDetection &&) = default;
  BlinkDetection &operator=(const BlinkDetection &) = delete;
  BlinkDetection &operator=(BlinkDetection &&) = default;
  virtual ~BlinkDetection() {}

  virtual void operator()(Event event) {
    _col = static_cast<int>(event.x / _xGridSize);
    _row = static_cast<int>(event.y / _yGridSize);

    _currentTimeStamp = event.timestamp;

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
          // in case activity has risen above lowerThreshold -> candidate
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
          // in case activity dropped below lower threshold -> register blink
          if (_grid[i][_row].activity < (_lowerThreshold - 10)) {
            std::cout << i << "/" << _row << "-" << '\n';
            _grid[i][_row].state = State::background;
            BlinkCandidate candidate;
            candidate.tileIndex = i;
            candidate.x = event.x;
            candidate.y = event.y;
            auto blinkBeginTimestamp = _grid[i][_row].blinkBeginTimestamp;
            candidate.duration = _currentTimeStamp - blinkBeginTimestamp;
            candidate.meanTimestamp =
                static_cast<long>(candidate.duration / 2) + blinkBeginTimestamp;
            _blinkCandidates[_row].push_back(candidate);
            // delete blinkBeginTimestamp;
            // delete candidate;
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

    // check whether mean timestamps of the last two blinking candidates
    // (eyes) are not more than 50ms apart and candidates are two tiles apart
    if (_blinkCandidates[_row].size() > 1 &&
        abs((_blinkCandidates[_row].rbegin() + 1)->meanTimestamp -
            _blinkCandidates[_row].back().meanTimestamp) < 50000 &&
        (abs(((_blinkCandidates[_row].rbegin() + 1)->tileIndex -
              _blinkCandidates[_row].back().tileIndex)) == 2
         //||
         // abs(((_blinkCandidateVector[_row].rbegin() + 1)->first -
         //_blinkCandidateVector[_row].back().first)) == 1
         )) {

      Blink blink;
      if ((_blinkCandidates[_row].rbegin() + 1)->tileIndex <
          _blinkCandidates[_row].back().tileIndex) {
        blink.leftEye = *(_blinkCandidates[_row].rbegin() + 1);
        blink.rightEye = _blinkCandidates[_row].back();
      } else {
        blink.leftEye = _blinkCandidates[_row].back();
        blink.rightEye = *(_blinkCandidates[_row].rbegin() + 1);
      }
      blink.centralTileIndex =
          (blink.leftEye.tileIndex + blink.rightEye.tileIndex) / 2;
      blink.centralTimestamp = static_cast<long>(
          (blink.leftEye.meanTimestamp - blink.rightEye.meanTimestamp) / 2);
      blink.duration = static_cast<long>(
          (blink.leftEye.duration + blink.rightEye.duration) / 2);

      _blinks[_row].push_back(blink);

      /*
      _blinkVector[_row].push_back(std::make_pair(
          static_cast<int>(((_blinkCandidates[_row].rbegin() + 1)->tileIndex +
                            _blinkCandidates[_row].back().tileIndex) /
                           2),
          ((_blinkCandidates[_row].rbegin() + 1)->meanTimestamp +
           _blinkCandidates[_row].back().meanTimestamp) /
              2));
              */

      if (_blinks[_row].size() > 1 &&
          (_blinks[_row].back().centralTileIndex -
           (_blinks[_row].rbegin() + 1)->centralTileIndex) < 1 &&
          800000 < (_blinks[_row].back().centralTimestamp -
                    (_blinks[_row].rbegin() + 1)->centralTimestamp) < 4000000) {
        _isBlink = true;
        event.x = _blinks[_row].back().leftEye.x;
        event.y = _blinks[_row].back().leftEye.y;
        _x2 = _blinks[_row].back().rightEye.x;
        _y2 = _blinks[_row].back().rightEye.y;
        std::cout << "_x2: " << _x2 << " and _y2: " << _y2 << '\n';
        _blinkCandidates[_row].clear();
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
    short tileIndex;
    short x;
    short y;
    long meanTimestamp;
    long duration;
  };

  struct Blink {
    BlinkCandidate leftEye;
    BlinkCandidate rightEye;
    float centralTileIndex;
    long centralTimestamp;
    long duration;
  };

  BlinkEventFromEvent _blinkEventFromEvent;
  const long _lifespan;
  double _lowerThreshold;
  double _upperThreshold;
  long _currentTimeStamp;
  short _xGridSize;
  short _yGridSize;
  short _col;
  short _row;
  short _x2;
  short _y2;
  bool _isBlink;
  std::vector<BlinkCandidate> _blinkCandidates[12];
  std::vector<Blink> _blinks[12];
  Tile _grid[16][12];
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
