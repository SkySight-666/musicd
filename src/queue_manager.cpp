#include "musicd/queue_manager.h"

namespace musicd {

void QueueManager::ReplaceQueue(const TrackList& tracks) {
  tracks_ = tracks;
  current_index_ = tracks_.empty() ? -1 : 0;
}

void QueueManager::Enqueue(const Track& track) {
  tracks_.push_back(track);
  if (current_index_ < 0) current_index_ = 0;
}

bool QueueManager::AdvanceToNext() {
  if (tracks_.empty()) return false;
  if (current_index_ + 1 >= static_cast<int>(tracks_.size())) return false;
  ++current_index_;
  return true;
}

void QueueManager::Clear() {
  tracks_.clear();
  current_index_ = -1;
}

const TrackList& QueueManager::tracks() const {
  return tracks_;
}

int QueueManager::current_index() const {
  return current_index_;
}

const Track* QueueManager::current_track() const {
  if (current_index_ < 0 || current_index_ >= static_cast<int>(tracks_.size())) return nullptr;
  return &tracks_[current_index_];
}

}  // namespace musicd
