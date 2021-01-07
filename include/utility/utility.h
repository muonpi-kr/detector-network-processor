#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <array>
#include <atomic>
#include <sstream>
#include <iomanip>


template<class T, class U>
auto has_plus_test(T&& t, U&& u) -> decltype(static_cast<void>(t + u), std::true_type{});

std::false_type has_plus_test(...);

template<class T, class U>
using has_plus = decltype(has_plus_test(std::declval<T>(), std::declval<U>()));


template<class T, class U>
auto has_minus_test(T&& t, U&& u) -> decltype(static_cast<void>(t - u), std::true_type{});

std::false_type has_minus_test(...);

template<class T, class U>
using has_minus = decltype(has_minus_test(std::declval<T>(), std::declval<U>()));


template<class T, class U>
auto has_mult_test(T&& t, U&& u) -> decltype(static_cast<void>(t * u), std::true_type{});

std::false_type has_mult_test(...);

template<class T, class U>
using has_multiplication = decltype(has_mult_test(std::declval<T>(), std::declval<U>()));


template<class T, class U>
auto has_div_test(T&& t, U&& u) -> decltype(static_cast<void>(t / u), std::true_type{});

std::false_type has_div_test(...);

template<class T, class U>
using has_division = decltype(has_div_test(std::declval<T>(), std::declval<U>()));

namespace MuonPi {

class MessageConstructor
{
public:
    /**
     * @brief MessageConstructor
     * @param delimiter The delimiter which separates the fields
     */
    MessageConstructor(char delimiter);

    /**
     * @brief add_field Adds a field to the complete message
     * @param field The field to add
     */
    void add_field(const std::string& field);

    /**
     * @brief get_string Gets the complete string
     * @return The completed string
     */
    [[nodiscard]] auto get_string() const -> std::string;

private:
    std::string m_message {};
    char m_delimiter;
};


class MessageParser
{
public:
    /**
     * @brief MessageParser
     * @param message The message to parse
     * @param delimiter The delimiter separating the fields in the message
     */
    MessageParser(const std::string& message, char delimiter);

    /**
     * @brief size
     * @return The number of fields in the message
     */
    [[nodiscard]] auto size() const -> std::size_t;
    /**
     * @brief empty
     * @return true if there are no fields
     */
    [[nodiscard]] auto empty() const -> bool;

    /**
     * @brief operator [] Access one field in the message
     * @param i The index of the field
     * @return The string contained in the field
     */
    [[nodiscard]] auto operator[](std::size_t i) const -> std::string;

    /**
     * @brief get Get the original string
     * @return The original string
     */
    [[nodiscard]] auto get() const -> std::string;
private:
    /**
     * @brief skip_delimiter Skips all delimiters until the next field
     */
    void skip_delimiter();
    /**
     * @brief read_field reads the next field
     */
    void read_field();
    /**
     * @brief at_end
     * @return True if the iterator is at the end of the string
     */
    [[nodiscard]] auto at_end() -> bool;

    std::string m_content {};
    std::string::iterator m_iterator;
    char m_delimiter {};

    std::vector<std::pair<std::string::iterator, std::string::iterator>> m_fields {};
};

template <std::size_t N, std::size_t T>
class RateMeasurement
{
public:
//	typedef RateMeasurement<N,T> Current;
    /**
     * @brief increase_counter Increases the counter in the current interval
     */
    void increase_counter();

    /**
     * @brief step Called periodically
     * @return True if the timeout was reached and the rates have been determined in this step
     */
    auto step() -> bool;

    /**
     * @brief current Get the current rate
     * @return The current rate. Might be a little unstable
     */
    [[nodiscard]] auto current() const noexcept -> double;

    /**
     * @brief mean The mean rate over the specified interval
     * @return The mean rate
     */
    [[nodiscard]] auto mean() const noexcept -> double;

    /**
     * @brief deviation Standard deviation
     * @return The standard deviation of all entries in the current interval used for the mean
     */
    [[nodiscard]] auto deviation() const noexcept -> double;

private:
    double m_current { 0.0 };
    double m_mean { 0.0 };
    double m_variance { 0.0 };
    double m_deviation { 0.0 };

    double m_full { false };

    std::array<double, N> m_history { 0.0 };

    std::size_t m_index { 0 };

    std::size_t m_current_n { 0 };
    std::chrono::steady_clock::time_point m_last { std::chrono::steady_clock::now() };
};


template <typename T, std::size_t N>
class Ringbuffer {
    static_assert(has_plus<T, std::size_t>::value);
    static_assert(has_minus<T, std::size_t>::value);
    static_assert(has_multiplication<T, std::size_t>::value);
    static_assert(has_division<T, std::size_t>::value);

    static_assert(has_plus<std::size_t, T>::value);
    static_assert(has_minus<std::size_t, T>::value);
    static_assert(has_multiplication<std::size_t, T>::value);
    static_assert(has_division<std::size_t, T>::value);

    static_assert(has_plus<T, T>::value);
    static_assert(has_minus<T, T>::value);
    static_assert(has_multiplication<T, T>::value);
    static_assert(has_division<T, T>::value);
public:
    void add(const T& val);
    [[nodiscard]] auto mean() const -> T;
    [[nodiscard]] auto stddev() const -> T;
    [[nodiscard]] auto variance() const -> T;
    [[nodiscard]] auto entries() const -> std::size_t;

private:
    std::array<T,N> m_buffer { T {} };
    std::size_t m_index;
    bool m_full { false };
};

/// Vertextet einen Ganzzahlwert val im Hexadezimalformat.
/// Auf die Minimal-Breite width wird mit führenden Nullen aufgefüllt;
/// falls nicht angegeben, wird diese Breite aus dem Typ des Arguments
/// abgeleitet. Funktion geeignet von char bis long long.
/// Zeiger, Fließkommazahlen u.ä. werden nicht unterstützt, ihre
/// Übergabe führt zu einem (beabsichtigten!) Compilerfehler.
/// Grundlagen aus: http://stackoverflow.com/a/5100745/2932052
template <typename T>
inline std::string int_to_hex(T val, std::size_t width=sizeof(T)*2)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(width) << std::hex << (val|0);
    return ss.str();
}


// +++++++++++++++++++++++++++++++
// implementation part starts here
// +++++++++++++++++++++++++++++++

// +++++++++++++++++++++++++++++++
// class RateMeasurement
template <std::size_t N, std::size_t T>
void RateMeasurement<N, T>::increase_counter()
{
    m_current_n = m_current_n + 1;
}

template <std::size_t N, std::size_t T>
auto RateMeasurement<N, T>::step() -> bool
{
    std::chrono::steady_clock::time_point now { std::chrono::steady_clock::now() };
    if (static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last).count()) >= T) {
        m_last = now;

        m_current = static_cast<double>(m_current_n) * 1000.0 / static_cast<double>(T);

        m_history[m_index] = m_current;
        m_index = (m_index + 1) % N;
        if (m_index == 0) {
            m_full = true;
        }
        if (!m_full) {
            m_mean = std::accumulate(m_history.begin(), m_history.begin()+m_index, 0.0) / m_index;
            m_variance = 1.0 / ( m_index - 1 ) * std::inner_product(m_history.begin(), m_history.begin()+m_index, m_history.begin(), 0.0,
                                               [](double const & x, double const & y) { return x + y; },
                                               [this](double const & x, double const & y) { return (x - m_mean)*(y - m_mean); });
        } else {
            m_mean = std::accumulate(m_history.begin(), m_history.end(), 0.0) / N;
            m_variance = 1.0 / ( N - 1.0 ) * std::inner_product(m_history.begin(), m_history.end(), m_history.begin(), 0.0,
                                               [](double const & x, double const & y) { return x + y; },
                                               [this](double const & x, double const & y) { return (x - m_mean)*(y - m_mean); });
        }
        m_deviation = std::sqrt( m_variance );

        m_current_n = 0;
        return true;
    }
    return false;
}

template <std::size_t N, std::size_t T>
auto RateMeasurement<N, T>::current() const noexcept -> double
{
    return m_current;
}

template <std::size_t N, std::size_t T>
auto RateMeasurement<N, T>::mean() const noexcept -> double
{
    return m_mean;
}

template <std::size_t N, std::size_t T>
auto RateMeasurement<N, T>::deviation() const noexcept -> double
{
    return m_deviation;
}
// -------------------------------


// +++++++++++++++++++++++++++++++
// class GUID
class GUID
{
public:
    GUID(std::size_t hash, std::uint64_t time);

    [[nodiscard]] auto to_string() const -> std::string;
    [[nodiscard]] static auto get_mac() -> std::uint64_t;

private:
    [[nodiscard]] static auto get_number() -> std::uint64_t;

    std::uint64_t m_first { 0 };
    std::uint64_t m_second { 0 };
};
// -------------------------------

// +++++++++++++++++++++++++++++++
// class Ringbuffer
template <typename T, std::size_t N>
void Ringbuffer<T,N>::add(const T& val) {
    m_buffer[m_index++]=val;
    if (m_index>=N) {
        m_index=0;
        m_full = true;
    }
}

template <typename T, std::size_t N>
auto Ringbuffer<T,N>::mean() const -> T
{
    T mean { };
    if (!m_full) {
        mean = std::accumulate(m_buffer.begin(), m_buffer.begin()+m_index, 0.0) / std::max<double>(m_index, 1.0);
    } else {
        mean = std::accumulate(m_buffer.begin(), m_buffer.end(), 0.0) / N;
    }
    return mean;
}

template <typename T, std::size_t N>
auto Ringbuffer<T,N>::stddev() const -> T
{
    return std::sqrt( this->variance );
}

template <typename T, std::size_t N>
auto Ringbuffer<T,N>::variance() const -> T
{
    T mean { this->mean() };
    T variance { };
    if (!m_full) {
        variance = 1.0 / ( m_index - 1 ) * std::inner_product(m_buffer.begin(), m_buffer.begin()+m_index, m_buffer.begin(), 0.0,
                                               [](T const & x, T const & y) { return x + y; },
                                               [mean](T const & x, T const & y) { return (x - mean)*(y - mean); });
    } else {
        variance = 1.0 / ( N - 1.0 ) * std::inner_product(m_buffer.begin(), m_buffer.end(), m_buffer.begin(), 0.0,
                                               [](T const & x, T const & y) { return x + y; },
                                               [mean](T const & x, T const & y) { return (x - mean)*(y - mean); });
    }
    return variance;
}

template <typename T, std::size_t N>
auto Ringbuffer<T,N>::entries() const -> std::size_t
{
    return ( (m_full)?N:m_index );
}
// -------------------------------

}
#endif // UTILITY_H
