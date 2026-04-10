#ifndef MUSICD_QUEUE_MANAGER_H_
#define MUSICD_QUEUE_MANAGER_H_

#include "musicd/types.h"

namespace musicd {

class QueueManager {
 public:
  void ReplaceQueue(const TrackList& tracks);
  void Enqueue(const Track& track);
  bool AdvanceToNext();
  void Clear();
  const TrackList& tracks() const;
  int current_index() const;
  const Track* current_track() const;

 private:
  TrackList tracks_;
  int current_index_ = -1;
};

}  // namespace musicd

#endif  // MUSICD_QUEUE_MANAGER_H_
