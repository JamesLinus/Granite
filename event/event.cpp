/* Copyright (c) 2017 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "event.hpp"
#include <algorithm>
#include <assert.h>

using namespace std;

namespace Granite
{

EventManager::~EventManager()
{
	dispatch();
	for (auto &event_type : latched_events)
		for (auto &handler : event_type.second.handlers)
			dispatch_down_events(event_type.second.queued_events, handler);
}

void EventManager::dispatch()
{
	for (auto &event_type : events)
	{
		auto &handlers = event_type.second.handlers;
		auto &queued_events = event_type.second.queued_events;
		auto itr = remove_if(begin(handlers), end(handlers), [&](const Handler &handler) {
			for (auto &event : queued_events)
				if (!handler.mem_fn(handler.handler, *event))
					return true;
			return false;
		});

		handlers.erase(itr, end(handlers));
		queued_events.clear();
	}
}

void EventManager::dispatch_event(std::vector<Handler> &handlers, const Event &e)
{
	auto itr = remove_if(begin(handlers), end(handlers), [&](const Handler &handler) {
		bool keep_event = handler.mem_fn(handler.handler, e);
		return !keep_event;
	});

	handlers.erase(itr, end(handlers));
}

void EventManager::dispatch_up_events(std::vector<std::unique_ptr<Event>> &events, const LatchHandler &handler)
{
	for (auto &event : events)
		handler.up_fn(handler.handler, *event);
}

void EventManager::dispatch_down_events(std::vector<std::unique_ptr<Event>> &events, const LatchHandler &handler)
{
	for (auto &event : events)
		handler.down_fn(handler.handler, *event);
}

void EventManager::LatchEventTypeData::flush_recursive_handlers()
{
	handlers.insert(end(handlers), begin(recursive_handlers), end(recursive_handlers));
	recursive_handlers.clear();
}

void EventManager::EventTypeData::flush_recursive_handlers()
{
	handlers.insert(end(handlers), begin(recursive_handlers), end(recursive_handlers));
	recursive_handlers.clear();
}

void EventManager::dispatch_up_event(LatchEventTypeData &event_type, const Event &event)
{
	event_type.dispatching = true;
	for (auto &handler : event_type.handlers)
		handler.up_fn(handler.handler, event);
	event_type.flush_recursive_handlers();
	event_type.dispatching = false;
}

void EventManager::dispatch_down_event(LatchEventTypeData &event_type, const Event &event)
{
	event_type.dispatching = true;
	for (auto &handler : event_type.handlers)
		handler.down_fn(handler.handler, event);
	event_type.flush_recursive_handlers();
	event_type.dispatching = false;
}

void EventManager::unregister_handler(EventHandler *handler)
{
	for (auto &event_type : events)
	{
		auto itr = remove_if(begin(event_type.second.handlers), end(event_type.second.handlers), [&](const Handler &h) {
			return h.unregister_key == handler;
		});

		if (itr != end(event_type.second.handlers) && event_type.second.dispatching)
			throw logic_error("Unregistering handlers while dispatching events.");

		if (itr != end(event_type.second.handlers))
			event_type.second.handlers.erase(itr, end(event_type.second.handlers));
	}
}

#if 0
// Check against mem_fn isn't safe.
void EventManager::unregister_handler(const Handler &handler)
{
	for (auto &event_type : events)
	{
		auto itr = remove_if(begin(event_type.second.handlers), end(event_type.second.handlers), [&](const Handler &h) {
			return h.unregister_key == handler.unregister_key && h.mem_fn == handler.mem_fn;
		});

		if (itr != end(event_type.second.handlers) && event_type.second.dispatching)
			throw logic_error("Unregistering handlers while dispatching events.");

		if (itr != end(event_type.second.handlers))
			event_type.second.handlers.erase(itr, end(event_type.second.handlers));
	}
}
#endif

void EventManager::unregister_latch_handler(EventHandler *handler)
{
	for (auto &event_type : latched_events)
	{
		auto itr = remove_if(begin(event_type.second.handlers), end(event_type.second.handlers), [&](const LatchHandler &h) {
			return h.unregister_key == handler;
		});

		if (itr != end(event_type.second.handlers))
			event_type.second.handlers.erase(itr, end(event_type.second.handlers));
	}
}

void EventManager::unregister_latch_handler(const LatchHandler &handler)
{
	for (auto &event_type : latched_events)
	{
		auto itr = remove_if(begin(event_type.second.handlers), end(event_type.second.handlers), [&](const LatchHandler &h) {
			bool signal = h.unregister_key == handler.unregister_key && h.up_fn == handler.up_fn && h.down_fn == handler.down_fn;
			if (signal)
				dispatch_down_events(event_type.second.queued_events, h);
			return signal;
		});

		if (itr != end(event_type.second.handlers) && event_type.second.dispatching)
			throw logic_error("Unregistering handlers while dispatching events.");

		if (itr != end(event_type.second.handlers))
			event_type.second.handlers.erase(itr, end(event_type.second.handlers));
	}
}

void EventManager::dequeue_latched(uint64_t cookie)
{
	for (auto &event_type : latched_events)
	{
		auto &events = event_type.second.queued_events;
		if (event_type.second.enqueueing)
			throw logic_error("Dequeueing latched while queueing events.");
		event_type.second.enqueueing = true;

		auto itr = remove_if(begin(events), end(events), [&](const unique_ptr<Event> &event) {
			bool signal = event->get_cookie() == cookie;
			if (signal)
				dispatch_down_event(event_type.second, *event);
			return signal;
		});

		event_type.second.enqueueing = false;
		events.erase(itr, end(events));
	}
}

void EventManager::dequeue_all_latched(EventType type)
{
	auto &event_type = latched_events[type];
	if (event_type.enqueueing)
		throw logic_error("Dequeueing latched while queueing events.");

	event_type.enqueueing = true;
	for (auto &event : event_type.queued_events)
		dispatch_down_event(event_type, *event);
	event_type.queued_events.clear();
	event_type.enqueueing = false;
}

EventHandler::~EventHandler()
{
	EventManager::get_global().unregister_handler(this);
	EventManager::get_global().unregister_latch_handler(this);
}
}
