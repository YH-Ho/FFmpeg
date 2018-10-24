#ifndef BLOCKINGQUEUE_H
#define BLOCKINGQUEUE_H

/* Use no less than c++11 standard */
#pragma once
#include <iostream>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

using namespace std;

template<typename T>
class BlockingQueue{
public:
    /* Delete the copy constructor and assignment operator */
    BlockingQueue(const BlockingQueue<T>&) = delete;
    BlockingQueue& operator=(const BlockingQueue<T>&) = delete;

    /* Constructor with parameter of queue max size */
    explicit BlockingQueue<T>(size_t max_size = 0) {
        if (max_size <= 0)
            m_max_size = SIZE_MAX;
        else
            m_max_size = max_size;
    }

    /* Get current queue size */
    size_t size() const{
        lock_guard<mutex> lk(m_mutex);
        return m_queue.size();
    }

    /* Determine whether queue is empty */
    bool empty() const{
        lock_guard<mutex> lk(m_mutex);
        return m_queue.empty();
    }

    /* Push data in queue */
    void push(T value) {
        unique_lock<mutex> lk(m_mutex);
        shared_ptr<T> data(make_shared<T>(move(value)));
        m_en_cond.wait(lk, [this] { return m_queue.size() < m_max_size; });
        m_queue.push(data);
        m_de_cond.notify_one();
    }

    /*  Wait and pop data out queue */
    void wait_and_pop(T& value) {
        unique_lock<mutex> lk(m_mutex);
        m_de_cond.wait(lk, [this] { return !m_queue.empty(); });
        value = move(*m_queue.front());
        m_queue.pop();
        m_en_cond.notify_one();
    }

    bool try_pop(T& value) {
        lock_guard<mutex> lk(m_mutex);
        if (m_queue.empty())
            return false;
        value = move(*m_queue.front());
        m_queue.pop();
        m_en_cond.notify_one();
        return true;
    }

    /* Clear the queue */
    void clear() {
        lock_guard<mutex> lk(m_mutex);
        queue<shared_ptr<T>> empty;
        swap(empty, m_queue);
    }

    shared_ptr<T> wait_and_pop() {
        unique_lock<mutex> lk(m_mutex);
        m_de_cond.wait(lk, [this] { return !m_queue.empty(); });
        shared_ptr<T> res = m_queue.front();
        m_queue.pop();
        m_en_cond.notify_one();
        return res;
    }

    shared_ptr<T> try_pop() {
        lock_guard<mutex> lk(m_mutex);
        if (m_queue.empty())
            return shared_ptr<T>();
        shared_ptr<T> res = m_queue.front();
        m_queue.pop();
        m_en_cond.notify_one();
        return res;
    }

private:
    mutable mutex m_mutex;
    size_t m_max_size;
    queue<shared_ptr<T>> m_queue;
    condition_variable m_de_cond;
    condition_variable m_en_cond;
};

#endif // BLOCKINGQUEUE_H
