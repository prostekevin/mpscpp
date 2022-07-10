#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "queue.hpp"
#include <tuple>
#include <optional>
#include <type_traits>
#include <stdexcept>

template <typename T> class Sender;
template <typename T> class Receiver;

template <typename T>
std::tuple<Sender<T>, Receiver<T>> make_channel();

template <typename T>
class Channel {
    private:
        Channel() {};
        threadsafe_queue<T> que;
        bool _closed = false;

        friend std::tuple<Sender<T>, Receiver<T>> make_channel<T>();

    public:

    void send(T&& val);
    void send(const T& val);

    void close_channel();
    bool closed();

    std::optional<T> recv();
    std::optional<T> try_recv();

    Channel<T> &operator=(const Channel<T>&)=delete;
    Channel<T> &operator=(Channel<T>&&)=delete;
    Channel(const Channel<T>&)=delete;
    Channel(Channel<T>&&)=delete;
};

template <typename T>
void Channel<T>::send(T&& val) {
    que.push(std::move(val));
}

template <typename T>
void Channel<T>::send(const T& val) {
    que.push(val);
}

template <typename T>
std::optional<T> Channel<T>::recv() {
    return *que.wait_and_pop();
}

template <typename T>
std::optional<T> Channel<T>::try_recv() {
    return *que.try_pop();
}

template <typename T>
void Channel<T>::close_channel() {
    _closed = true;
}

template <typename T>
bool Channel<T>::closed() {
	return _closed;
}

template <typename T>
class Sender {
    private:
        std::shared_ptr<Channel<T>> channel;

        Sender(std::shared_ptr<Channel<T>> ch)
            : channel(ch) {};
        
        void moved() {
            if (!channel)
                throw std::logic_error("Sender has been moved.");
        }

        friend std::tuple<Sender<T>, Receiver<T>> make_channel<T>();

    public:
        Sender<T>& send(T&& val);
        Sender<T>& send(const T& val);
        void close();
        bool closed();

};

template <typename T>
Sender<T>& Sender<T>::send(T&& val) {
    moved();
    channel->send(std::move(val));
    return *this;
}

template <typename T>
Sender<T>& Sender<T>::send(const T& val) {
    moved();
    channel->send(val);
    return *this;
}

template <typename T>
void Sender<T>::close() {
    moved();
    channel->close_channel();
}

template <typename T>
bool Sender<T>::closed() {
    moved();
    return channel->closed();
}

template<typename T>
class Receiver {
    private:
        std::shared_ptr<Channel<T>> channel;

        Receiver(std::shared_ptr<Channel<T>> ch)
            : channel(ch) {};

        void moved() {
            if (!channel)
                throw std::logic_error("Receiver has been moved.");
        }

        friend std::tuple<Sender<T>, Receiver<T>> make_channel<T>();

    public:
        std::optional<T> recv();
        std::optional<T> try_recv();
        bool closed();

        Receiver(const Receiver<T>&)=delete;
        Receiver<T>& operator=(const Receiver<T>&)=delete;

        Receiver(Receiver<T>&&) = default;
	    Receiver<T>& operator=(Receiver<T>&&) = default;

        class iterator : public std::iterator<std::input_iterator_tag, T> {
            private:
                typedef std::iterator<std::input_iterator_tag, T> Iter;
		        Receiver<T>* receiver;
		        std::optional<T> current = std::nullopt;
		        void next();

            public:
                using typename Iter::iterator_category;
                using typename Iter::value_type;
                using typename Iter::reference;
                using typename Iter::pointer;
                using typename Iter::difference_type;

                iterator(): receiver(nullptr) {}
		        iterator(Receiver<T>& receiver): receiver(&receiver) {
			        if (this->receiver->closed())
                         this->receiver = nullptr;
			        else next();
		        }
	
		        reference operator*() { return current.value(); }
		        pointer operator->() { return &current.value(); }
		        iterator& operator++() {
			        next();
			        return *this;
		        }
		        iterator operator++(int) = delete;
		        bool operator==(iterator& other) {
			        if (receiver == nullptr && other.receiver == nullptr) return true;
			        return false;
		        }
		        bool operator!=(iterator& other) {
			        return !(*this == other);
		        }
        };

        iterator begin() {
		    return iterator(*this);
	    }
	
	    iterator end() {
		    return iterator();
	    }
};

template<typename T>
std::optional<T> Receiver<T>::recv() {
    moved();
    return channel->recv();
}

template<typename T>
std::optional<T> Receiver<T>::try_recv() {
    moved();
    return channel->try_recv();
}

template<typename T>
bool Receiver<T>::closed() {
    moved();
    return channel->closed();
}

template <typename T>
std::tuple<Sender<T>, Receiver<T>> make_channel() {
	static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>, "type not movable or copyable.");
	std::shared_ptr<Channel<T>> channel{new Channel<T>()};
	Sender<T> sender{channel};
	Receiver<T> receiver{channel};
	return std::tuple<Sender<T>, Receiver<T>>{
		std::move(sender),
		std::move(receiver)
	};
}

template <typename T>
void Receiver<T>::iterator::next() {
	if (!receiver)
         return;
	while(true) {
		if (receiver->closed()) {
			receiver = nullptr;
			current.reset();
			return;
		}
		std::optional<T> tmp = receiver->recv();
		if (!tmp.has_value()) continue;
		current.emplace(std::move(tmp.value()));
		return;
	}
}


#endif