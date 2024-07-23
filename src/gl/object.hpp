#ifndef SHADOW_CAST_GL_OBJECT_HPP_INCLUDED
#define SHADOW_CAST_GL_OBJECT_HPP_INCLUDED

#include "utils/contracts.hpp"
#include <GL/gl.h>
#include <type_traits>
#include <utility>

namespace sc
{
namespace opengl
{

// clang-format off
template<typename T>
concept BasicTraits =
        std::is_default_constructible_v<T> &&
        requires (T val) {

    typename T::category;
};

template<typename T>
concept ObjectTraits =
        BasicTraits<T> &&
        requires(T val, GLuint name) {

    /* FIX:
     *  How can we specify create here if it can be called with
     *  zero or more arguments?...
     */
    //{ val.create() };
    { val.destroy(name) };
};

template<typename T>
concept TargetTraits =
        BasicTraits<T> &&
        requires(T val, GLuint name, GLenum target_name) {

    { val.bind(target_name, name) };
};

template<typename T>
concept Target = requires (T val) {
    typename T::category;

    requires TargetTraits<typename T::traits_type>;
    { val.target() };
};

template<typename T>
concept Object = requires (T val) {
    typename T::category;
    typename T::traits_type;

    requires ObjectTraits<typename T::traits_type>;
    { val.name() };
};
// clang-format on

template <ObjectTraits Traits>
struct ObjectBase
{
    static constexpr GLuint no_name = static_cast<GLuint>(0);

    using traits_type = Traits;
    using name_type = GLuint;
    using category = typename traits_type::category;

    ObjectBase() noexcept = default;
    explicit ObjectBase(GLuint name) noexcept
        : name_ { name }
    {
    }

    ObjectBase(ObjectBase&& other) noexcept
        : name_ { std::exchange(other.name_, no_name) }
    {
    }

    ~ObjectBase()
    {
        if (name_ == no_name)
            return;

        traits_.destroy(name_);
    }

    auto operator=(ObjectBase&& other) noexcept -> ObjectBase&
    {
        using std::swap;
        auto tmp { std::move(other) };
        swap(*this, tmp);
        return *this;
    }

    auto name() const noexcept -> name_type { return name_; }

    template <ObjectTraits T_>
    friend auto swap(ObjectBase<T_>& lhs, ObjectBase<T_>& rhs) noexcept -> void;

    explicit operator GLuint() const noexcept { return name_; }

private:
    name_type name_ { no_name };
    traits_type traits_ {};
};

template <ObjectTraits Traits>
auto swap(ObjectBase<Traits>& lhs, ObjectBase<Traits>& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.name_, rhs.name_);
}

template <GLenum T>
struct Enum
{
    static constexpr GLenum value = T;
    constexpr operator GLenum() const noexcept { return value; }
};

template <GLenum TargetName, TargetTraits Traits>
struct TargetBase
{
    using traits_type = Traits;
    using category = typename traits_type::category;

    static constexpr Enum<TargetName> target_name {};

    constexpr auto target() const noexcept { return target_name; }

    template <Object T>
    auto bind(T& object) const -> void
    requires(std::is_convertible_v<typename T::category,
                                   typename traits_type::category>)
    {
        traits_.bind(target_name, object.name());
    }

    auto unbind() const -> void { traits_.bind(target_name, 0); }

private:
    traits_type traits_ {};
};

template <Object T, typename... Args>
auto create(Args&&... args) -> T
{
    typename T::traits_type traits {};
    return T { traits.create(std::forward<Args>(args)...) };
}

template <Target T, Object U>
struct BoundTarget
{
    using target_type = std::decay_t<T>;
    using object_type = std::decay_t<U>;
    using category = typename target_type::category;

    BoundTarget(T const& target, U& object) noexcept
    requires(std::is_convertible_v<typename T::category, typename U::category>)
        : target_ { target }
        , object_ { object }
    {
        SC_EXPECT(object_.name() != 0);
        target_.bind(object);
    }

    BoundTarget(BoundTarget const& other) = delete;
    auto operator=(BoundTarget const& other) -> BoundTarget& = delete;

    ~BoundTarget()
    {
        if (object_.name() != 0)
            target_.unbind();
    }

    auto object() noexcept -> object_type& { return object_; }
    auto object() const noexcept -> object_type const& { return object_; }
    auto name() const noexcept -> GLuint { return object_.name(); }
    constexpr auto target() const noexcept { return target_.target_name; }
    auto unbind() -> void { target_.unbind(); }
    auto is_bound() const noexcept -> bool { return object_.name() != 0; }

private:
    target_type target_;
    object_type& object_;
};

template <typename T>
struct IsBinding : std::false_type
{
};

template <Target T, Object O>
struct IsBinding<BoundTarget<T, O>> : std::true_type
{
};

template <typename T>
constexpr auto BindingConcept = IsBinding<T>::value;

template <Target T, Object O>
[[nodiscard]] auto bind(T const& target, O& obj)
requires(!std::is_const_v<std::remove_reference_t<O>>)
{
    return BoundTarget { target, obj };
}

template <Target T, Object O, typename F>
auto bind(T const& target, O& obj, F f)
requires(!std::is_const_v<std::remove_reference_t<O>>)
{
    f(BoundTarget { target, obj });
}

template <Target T, Object O>
auto unbind(BoundTarget<T, O>&& bound_target) -> void
{
    bound_target.unbind();
}

template <Target T>
auto unbind(T&& target) -> void
{
    target.unbind();
}

} // namespace opengl

} // namespace sc

#endif // SHADOW_CAST_GL_OBJECT_HPP_INCLUDED
