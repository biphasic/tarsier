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
        _yGridSize(20), _isBlink(false), _x2(0), _y2(0), _counter(0) {}

  BlinkDetection(const BlinkDetection &) = delete;
  BlinkDetection(BlinkDetection &&) = default;
  BlinkDetection &operator=(const BlinkDetection &) = delete;
  BlinkDetection &operator=(BlinkDetection &&) = default;
  virtual ~BlinkDetection() {}

  virtual void operator()(Event event) {
    if (event.timestamp > 500000 && event.x != 304 && event.y != 240) {
      // std::cout << _counter++ << '\n';
      _col = static_cast<int>(event.x / _xGridSize);
      _row = static_cast<int>(event.y / _yGridSize);

      _currentTimeStamp = event.timestamp;
      _lastTimeStamp = _latestTimestampGrid[_col][_row];

      // updating activities
      for (size_t i = 0; i <= 15; i++) {
        // only update tiles that are beyond the lower threshold. Values below
        // are of no interest to an exponential decay
        if (_activityGrid[i][_row] > _lowerThreshold || i == _col) {
          _activityGrid[i][_row] *=
              exp(-static_cast<double>(_currentTimeStamp -
                                       _latestTimestampGrid[i][_row]) /
                  _lifespan);
          _latestTimestampGrid[i][_row] = _currentTimeStamp;
          if (i == _col) {
            // increase activity only in currently focused tile
            _activityGrid[i][_row] += 1;
          }

          // state machine
          switch (_blinkIndicatorGrid[i][_row]) {
          // BACKGROUND STATE
          case 1:
            // see if activity has risen above lowerThreshold to set blink
            // candidate, state to 2
            if (_activityGrid[i][_row] > _lowerThreshold) {
              _blinkIndicatorGrid[i][_row] = 2;
              _blinkBeginTSGrid[i][_row] = _currentTimeStamp;
              std::cout << i << "/" << _row << "+" << '\n';
            } else if (_activityGrid[i][_row] > _upperThreshold) {
              std::cout << "This shouldn't have happened" << '\n';
            }
            break;
          // POSSIBLE BLINK STATE
          case 2:
            // check whether activity may dropped below threshold plus some
            // safety margin
            if (_activityGrid[i][_row] < (_lowerThreshold - 10)) {
              std::cout << i << "/" << _row << "-" << '\n';
              _blinkIndicatorGrid[i][_row] = 1;
              _blinkCandidateVector[_row].push_back(std::make_pair(
                  i, static_cast<long>((_latestTimestampGrid[i][_row] -
                                        _blinkBeginTSGrid[i][_row]) /
                                       2) +
                         _blinkBeginTSGrid[i][_row]));
            } else if (_activityGrid[i][_row] > _upperThreshold) {
              _blinkIndicatorGrid[i][_row] = 3; // considered clutter now
              std::cout << i << "/" << _row << "[++]" << '\n';
            }
            break;
          // CLUTTER STATE
          case 3:
            if (_activityGrid[i][_row] < _lowerThreshold) {
              _blinkIndicatorGrid[i][_row] = 1;
              std::cout << i << "/" << _row << "[--]" << '\n';
            }
            break;
          default:
            // TODO refactor
            _blinkIndicatorGrid[i][_row] = 1;
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
            static_cast<int>(
                ((_blinkCandidateVector[_row].rbegin() + 1)->first +
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
  }

protected:
  BlinkEventFromEvent _blinkEventFromEvent;
  const uint64_t _lifespan;
  uint64_t _currentTimeStamp;
  uint64_t _lastTimeStamp;
  // double _activity;
  unsigned short int _xGridSize;
  unsigned short int _yGridSize;
  unsigned short int _col;
  unsigned short int _row;
  std::vector<unsigned short int> otherBlink;
  std::vector<std::pair<int, long>> _blinkCandidateVector[12];
  std::vector<std::pair<int, long>> _blinkVector[12];
  long _latestTimestampGrid[16][12];
  double _activityGrid[16][12];
  int _blinkIndicatorGrid[16][12];
  long _blinkBeginTSGrid[16][12];
  bool _isBlink;
  int _x2;
  int _y2;
  uint64_t _lowerThreshold;
  uint64_t _upperThreshold;
  long _counter;
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
