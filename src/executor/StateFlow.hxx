/** \copyright
 * Copyright (c) 2013, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file StateFlow.hxx
 *
 * Defines a type of state machine flow used within class Service.
 *
 * @author Stuart W Baker
 * @date 25 December 2013
 */

#ifndef _StateFlow_hxx_
#define _StateFlow_hxx_

#include <type_traits>

#include "executor/Allocator.hxx"
#include "executor/Message.hxx"
#include "utils/BufferQueue.hxx"

#define STATE(_fn)                                                             \
    (Callback)(&std::remove_reference<decltype(*this)>::type::_fn)

/** Begin the definition of a StateFlow.
 * @param _name the class name of the StateFlow derived object
 * @param _priorities number of input queue priorities
 */
#define STATE_FLOW_START(_name, _priorities)                                   \
    class _name : public StateFlow<_priorities>                                \
    {                                                                          \
    public:                                                                    \
        _name(Service *service) : StateFlow<_priorities>(service)              \
        {                                                                      \
        }                                                                      \
                                                                               \
        ~_name()                                                               \
        {                                                                      \
        }                                                                      \
                                                                               \
    private:                                                                   \
        Action entry(Message *msg);                                            \
                                                                               \
        _name();                                                               \
                                                                               \
    DISALLOW_COPY_AND_ASSIGN(_name)

/** Begin the definition of a StateFlow that includes timeouts.
 * @param _name the class name of the StateFlow derived object
 * @param _priorities number of input queue priorities
 */
#define STATE_FLOW_START_WITH_TIMER(_name, _priorities)                        \
    class _name : public StateFlow<_priorities>                                \
    {                                                                          \
    public:                                                                    \
        _name(Service *service)                                                \
            : StateFlow<_priorities>(service),                                 \
              timer(TIMEOUT_FROM(service, state_flow_timeout), service, this), \
              timerMsg(NULL)                                                   \
        {                                                                      \
        }                                                                      \
                                                                               \
        ~_name()                                                               \
        {                                                                      \
        }                                                                      \
                                                                               \
        void timeout()                                                         \
        {                                                                      \
            timerMsg ? me()->send(timerMsg) :;                                 \
            timerMsg = NULL;                                                   \
        }                                                                      \
                                                                               \
        void trigger()                                                         \
        {                                                                      \
            timer.trigger();                                                   \
        }                                                                      \
                                                                               \
    private:                                                                   \
        Action entry(Message *msg);                                            \
                                                                               \
        Action timeout_and_call(Callback c, Message *msg, long long period)    \
        {                                                                      \
            msg->id(msg->id() | Message::IN_PROCESS_MSK);                      \
            timerMsg = msg;                                                    \
            timer.start(period);                                               \
            return Action(c);                                                  \
        }                                                                      \
                                                                               \
        Timer timer;                                                           \
        Message *timerMsg;                                                     \
                                                                               \
        bool early()                                                           \
        {                                                                      \
            return timer.early();                                              \
        }                                                                      \
                                                                               \
        _name();                                                               \
                                                                               \
    DISALLOW_COPY_AND_ASSIGN(_name)

/** Declare a state callback in a StateFlow.
 * @param _state the method name of the StateFlow state callback
 */
#define STATE_FLOW_STATE(_state) Action _state()

/** End the definition of a StateFlow.
 */
#define STATE_FLOW_END() }

/** Runs incoming Messages through a State Flow.
 */
class StateFlowBase : public Executable
{
public:
    /** Callback from the executor. This function will be invoked when the
     * current stateflow gets the CPU. It will execute the current states until
     * the flow yields or is blocked in a waiting state. */
    virtual void run();

    /** Wakeup call arrived. Schedules *this on the executor. Does not know the
     * priority. */
    virtual void notify();

protected:
    /** Constructor.
     * @param service Service that this state flow is part of
     * @param size number of queues in the list
     */
    StateFlowBase(Service *service)
        : service_(service), state_(STATE(terminated))
    {
    }

    /** Destructor.
     */
    ~StateFlowBase()
    {
    }

    /* forward prototype */
    class Action;

    /** State Flow callback prototype
     */
    typedef Action (StateFlowBase::*Callback)();

    /** Return type for a state flow callback.
     */
    class Action
    {
    public:
        /** Constructor.
         */
        Action(Callback s) : nextState_(s)
        {
        }

        /** Get the next state for the StateFlowAction.
         */
        Callback next_state()
        {
            return nextState_;
        }

    private:
        /** next state in state flow */
        Callback nextState_;
    };

    /** Resets the flow to the specified state.
     * @param c is the state to continue the flow from after the next
     * notification.
     */
    void reset_flow(Callback c)
    {
        state_ = c;
    }

    /** Resets the flow to the specified state and starts it.
     * @param c is the state to start the flow from.
     */
    void start_flow(Callback c)
    {
        HASSERT(state_ == STATE(terminated));
        yield_and_call(c);
    }

    /*========== ACTION COMMANDS ===============*/
    /* StateFlow implementations will have to use one of the following commands
     * to return from a state handler to indicate what action to take. */

    /** Call the current state again.
     * @return function pointer to current state handler
     */
    Action again()
    {
        return Action(state_);
    }

    /** Terminate current StateFlow activity.  The message instance is not
     * released before termination.  This is usefull if the message will be
     * reused for the purpose of sending to another StateFlow.
     * @return function pointer to terminated method
     */
    Action exit()
    {
        return STATE(terminated);
    }

    /** Terminate current StateFlow activity. after releasing the message.
     * @param msg to release
     * @return function pointer to terminated method
     */
    Action release_and_exit(Message *msg)
    {
        msg->free();
        return exit();
    }

    /** Imediately call the next state upon return.
     * @param c Callback "state" to move to
     * @return function pointer to be returned from state function
     */
    Action call_immediately(Callback c)
    {
        return Action(c);
    }

    /** Wait for an asynchronous call.
     * @return special function pointer to return from a state handler that
     * will cause the StateFlow to wait for an incoming wakeup (notification).
     */
    Action wait()
    {
        return Action(nullptr);
    }

    /** Wait for resource to become available before proceeding to next state.
     * @param c State to move to
     * @return function pointer to be returned from state function
     */
    Action wait_and_call(Callback c)
    {
        state_ = c;
        return wait();
    }

    /** Wait for resource to become available before proceeding to next state.
     * if an immediate allocation can be made, an immediate call to the next
     * state will be made.
     * @param c Callback "state" to move to
     * @param msg Message instance we are waiting on
     * @return function pointer to be returned from state function
     */
    template <class T>
    Action allocate_and_call(Allocator<T> *a, Callback c, Message *msg)
    {
        return a->allocate_immediate(msg) ? call_immediately(c) : Action(c);
    }

    /** Place the current flow to the back of the executor, and transition to a
     * new state after we get the CPU again.  Similar to @ref call_immediately,
     * except we place this flow on the back of the Executor queue.
     * @param c Callback "state" to move to
     * @return function pointer to be returned from state function
     */
    Action yield_and_call(Callback c)
    {
        state_ = c;
        notify();
        return wait();
    }

    /** Return a pointer to the service I am bound to.
     * @return pointer to service
     */
    Service *service()
    {
        return service_;
    }

    /** Timeout expired.  The expectation is that a derived class will implement
     * this if it desires timeouts.
     */
    virtual void timeout()
    {
        HASSERT(0 && "unexpected timeout call arrived");
    }

private:
    /** Service this StateFlow belongs to */
    Service *service_;

    /** Terminate current StateFlow activity.  This method only exists for the
     * purpose of providing a unique address pointer.
     * @param msg unused
     * @return should never return
     */
    Action terminated(Message *msg);

    /** current active state in the flow */
    Callback state_;

    /** Default constructor.
     */
    StateFlowBase();

    DISALLOW_COPY_AND_ASSIGN(StateFlowBase);
};

/** A state flow that has an incoming message queue, pends on that queue, and
 * runs a flow for every message that comes in from that queue. */
class StateFlowWithQueue : public StateFlowBase, private Lockable
{
protected:
    StateFlowWithQueue(Service *service)
        : StateFlowBase(service), currentMessage_(nullptr)
    {
        reset_flow(STATE(wait_for_message));
    }

    /** Entry into the StateFlow activity.  Pure virtual which must be defined
     * by derived class. Must eventually (through some number of states) call
     * release_and_exit() to transition to getting next message.
     * @return function pointer to next state
     */
    virtual Action entry() = 0;

    /** Accessor to the queue implementation of the flow.
     * @returns the queue for the flow.
     */
    virtual AbstractQueue *queue() = 0;

    /** Terminates the processing of the current message. Flows should end with
     * this action. Frees the current message.
     * @return the action for checking for new messages.
     */
    Action release_and_exit()
    {
        currentMessage_->free();
        currentMessage_ = nullptr;
        return call_immediately(STATE(wait_for_message));
    }

    /// @returns the current message we are processing.
    Message* message() {
        return currentMessage_;
    }

private:
    STATE_FLOW_STATE(wait_for_message);

    /// Wakeup call arrived. Schedules *this on the executor.
    virtual void notify();

    /// Message we are currently processing.
    Message *currentMessage_;
    static const unsigned MAX_PRIORITY = 0x7FFFFFFFU;
    /// Priority of the current message we are processing.
    unsigned currentPriority_ : 31;
    /** True if we are in the pending state, waiting for an entry to show up in
     * the queue. Protected by Lockable *this. */
    unsigned isWaiting_ : 1;
};

template <class MessageType, class QueueType> class StateFlow : public StateFlowWithQueue
{
public:
    /** Constructor.
     * @param service Service that this state flow is part of
     * @param size number of queues in the list
     */
    StateFlow(Service *service)
        : StateFlowWithQueue(service)
    {
    }

    /** Destructor.
     */
    ~StateFlow()
    {
    }

    /** Sends a message to the state flow for processing. This function never
     * blocks.
     *
     * @param msg Message to enqueue
     * @param priority the priority at which to enqueue this message.
     */
    void send(const MessageType *msg, unsigned priority = UINT_MAX)
    {
        LockHolder h(this);
        queue_->send(msg, priority);
        if (isWaiting_) {
            isWaiting_ = 0;
            currentPriority_ = priority;
            this->notify();
        }
    }

    /** Signals that the asynchronous call / wait is over. Will schedule *this
     * on the executor. */
    virtual void notify();

protected:
    virtual AbstractQueue *queue() {
        return queue_;
    }

    /** @returns the current message we are processing. */
    const MessageType *message()
    {
        return static_cast<MessageType *>(StateFlowWithQueue::message());
    }

    /** Entry into the StateFlow activity.  Pure virtual which must be
     * defined by derived class.
     * @param msg Message belonging to the state flow
     * @return function pointer to next state
     */
    virtual Action entry(Message *msg) = 0;

private:
    /** Implementation of the queue. */
    QueueType queue_;
};

#endif /* _StateFlow_hxx_ */