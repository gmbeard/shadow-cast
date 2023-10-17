#ifndef SHADOW_CAST_RESULT_HPP_INCLUDED
#define SHADOW_CAST_RESULT_HPP_INCLUDED

#include <type_traits>
#include <variant>

namespace sc
{

namespace detail
{

struct VoidResult
{
    VoidResult() noexcept = default;

    /* Allow implicit conversion from any type. This
     * ensures we can coerce a Result<T, E> type to a
     * Result<void, E> type...
     */
    template <typename T>
    VoidResult(T&&) noexcept
    {
    }
};

template <typename E, typename R = VoidResult>
requires(!std::is_same_v<E, R>)
struct ResultStorage
{
    template <typename U, typename V>
    requires(!std::is_same_v<U, V>)
    friend struct ResultStorage;

    using ErrorType = E;
    using ResultType = R;

    ResultStorage()
    requires(std::is_same_v<R, VoidResult>)
        : storage_ { VoidResult {} }
    {
    }

    template <typename U>
    ResultStorage(U&& val)
    requires(std::is_convertible_v<U, ErrorType> ||
             std::is_convertible_v<U, ResultType>)
        : storage_ { std::forward<U>(val) }
    {
    }

    template <typename U, typename V>
    ResultStorage(ResultStorage<U, V> const& other)
    requires(std::is_convertible_v<U, ErrorType> &&
             std::is_convertible_v<V, ResultType>)
        : storage_ { other.storage_ }
    {
    }

    template <typename U, typename V>
    ResultStorage(ResultStorage<U, V>&& other)
    requires(std::is_convertible_v<U, ErrorType> &&
             std::is_convertible_v<V, ResultType>)
        : storage_ { std::move(other.storage_) }
    {
    }

    [[nodiscard]] auto is_error_value() const noexcept
    {
        return std::holds_alternative<ErrorType>(storage_);
    }

    [[nodiscard]] auto is_result_value() const noexcept
    {
        return !is_error_value();
    }

    [[nodiscard]] operator bool() const noexcept { return is_result_value(); }

    [[nodiscard]] auto error() const& noexcept -> ErrorType const&
    {
        return std::get<0>(storage_);
    }

    [[nodiscard]] auto error() & noexcept -> ErrorType&
    {
        return std::get<0>(storage_);
    }

    [[nodiscard]] auto error() && noexcept -> ErrorType&&
    {
        return std::get<0>(std::move(storage_));
    }

    [[nodiscard]] auto operator*() const noexcept -> ResultType const&
    requires(!std::is_same_v<ResultType, VoidResult>)
    {
        return std::get<1>(storage_);
    }

    [[nodiscard]] auto operator*() noexcept
        -> ResultType& requires(!std::is_same_v<ResultType, VoidResult>) {
                           return std::get<1>(storage_);
                       }

    [[nodiscard]] auto value() const& noexcept -> ResultType const&
    requires(!std::is_same_v<ResultType, VoidResult>)
    {
        return std::get<1>(storage_);
    }

    [[nodiscard]] auto value() & noexcept
        -> ResultType& requires(!std::is_same_v<ResultType, VoidResult>) {
                           return std::get<1>(storage_);
                       }

    [[nodiscard]] auto value() && noexcept
        -> ResultType&& requires(!std::is_same_v<ResultType, VoidResult>) {
                            return std::get<1>(std::move(storage_));
                        }

    private : std::variant<ErrorType, ResultType> storage_;
};

template <typename... Ts>
struct ResultTraits;

template <typename E>
struct ResultTraits<E>
{
    using type = ResultStorage<E>;
};

template <typename R, typename E>
struct ResultTraits<R, E>
{
    using type = ResultStorage<E, R>;
};

template <typename E>
struct ResultTraits<void, E>
{
    using type = ResultStorage<E>;
};

} // namespace detail

template <typename... Ts>
using Result = typename detail::ResultTraits<Ts...>::type;

[[nodiscard]] auto result_ok() noexcept -> detail::VoidResult;

template <typename R>
[[nodiscard]] auto result_ok(R&& val) noexcept
{
    return std::forward<R>(val);
}

template <typename E>
[[nodiscard]] auto result_error(E&& val) noexcept
{
    return std::forward<E>(val);
}

template <typename E, typename R>
[[nodiscard]] decltype(auto)
get_error(detail::ResultStorage<R, E> const& result)
{
    return result.error();
}

template <typename E, typename R>
[[nodiscard]] decltype(auto) get_error(detail::ResultStorage<R, E>& result)
{
    return result.error();
}

template <typename E, typename R>
[[nodiscard]] decltype(auto) get_error(detail::ResultStorage<R, E>&& result)
{
    return std::move(result).error();
}

template <typename E, typename R>
[[nodiscard]] decltype(auto)
get_value(detail::ResultStorage<R, E> const& result)
requires(!std::is_same_v<detail::VoidResult,
                         typename detail::ResultStorage<R, E>::ResultType>)
{
    return result.value();
}

template <typename E, typename R>
[[nodiscard]] decltype(auto) get_value(detail::ResultStorage<R, E>& result)
requires(!std::is_same_v<detail::VoidResult,
                         typename detail::ResultStorage<R, E>::ResultType>)
{
    return result.value();
}

template <typename E, typename R>
[[nodiscard]] decltype(auto) get_value(detail::ResultStorage<R, E>&& result)
requires(!std::is_same_v<detail::VoidResult,
                         typename detail::ResultStorage<R, E>::ResultType>)
{
    return std::move(result).value();
}

} // namespace sc

#endif // SHADOW_CAST_RESULT_HPP_INCLUDED
