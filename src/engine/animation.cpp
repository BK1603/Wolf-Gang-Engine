#define ENGINE_INTERNAL

#include <engine/renderer.hpp>
#include <engine/utility.hpp>

using namespace engine;


animation::animation()
{
	mTexture = nullptr;
	mDefault_frame = 0;
	mFrame_count = 0;
	mLoop = e_loop::linear;
}

void
animation::set_loop(e_loop pLoop)
{
	mLoop = pLoop;
}

animation::e_loop
animation::get_loop() const
{
	return mLoop;
}

void
animation::add_interval(frame_t pFrom, int pInterval)
{
	if (get_interval(pFrom) != pInterval)
		mSequence.push_back({ pInterval, pFrom });
}

int
animation::get_interval(frame_t pAt) const
{
	int retval = 0;
	frame_t last = 0;
	for (auto& i : mSequence)
	{
		if (i.from <= pAt && i.from >= last)
		{
			retval = i.interval;
			last = i.from;
		}
	}
	return retval;
}

void
animation::set_frame_count(frame_t pCount)
{
	mFrame_count = pCount;
}

frame_t
animation::get_frame_count() const
{
	return mFrame_count;
}

void animation::set_frame_rect(engine::frect pRect)
{
	mFrame_rect = pRect;
}

engine::frect
animation::get_frame_at(frame_t pAt) const
{
	if (mFrame_count == 0)
		return mFrame_rect;

	engine::frect ret = mFrame_rect;
	ret.x += ret.w*calculate_frame(pAt);
	return ret;
}

fvector
animation::get_size() const
{
	return mFrame_rect.get_size();
}

void
animation::set_default_frame(frame_t pFrame)
{
	mDefault_frame = pFrame;
}

int
animation::get_default_frame() const
{
	return mDefault_frame;
}

void
animation::set_texture(texture& pTexture)
{
	mTexture = &pTexture;
}

engine::texture*
animation::get_texture() const
{
	return mTexture;
}

frame_t
animation::calculate_frame(frame_t pCount) const
{
	if (mFrame_count == 0)
		return 0;
	switch (mLoop)
	{
	case animation::e_loop::none:
		return pCount >= mFrame_count ? mFrame_count - 1 : pCount;

	case animation::e_loop::linear:
			return pCount%mFrame_count;

	case animation::e_loop::pingpong:
			return util::pingpong_index(pCount, mFrame_count - 1);
	}
	return 0;
}

animation_node::animation_node()
{
	mAnchor = anchor::topleft;
	mPlaying = false;
	mAnimation = nullptr;
	add_child(mSprite);
}

void
animation_node::set_frame(frame_t pFrame)
{
	mFrame = pFrame;
	mClock.restart();
	mInterval = mAnimation->get_interval(mFrame);
	update_frame();
}

void
animation_node::set_animation(const animation& pAnimation, bool pSwap)
{
	mAnimation = &pAnimation;
	mInterval = pAnimation.get_interval();

	if (!pSwap)
		set_frame(pAnimation.get_default_frame());
	else
		update_frame();

	if (pAnimation.get_texture())
		set_texture(*pAnimation.get_texture());
}

void
animation_node::set_texture(texture& pTexture)
{
	mSprite.set_texture(pTexture);
}

engine::fvector animation_node::get_size() const
{
	if (!mAnimation)
		return{ 0, 0 };
	return mAnimation->get_size();
}

bool animation_node::tick()
{
	if (!mAnimation) return false;

	int time = mClock.get_elapse().ms_i();
	if (time >= mInterval && mInterval > 0)
	{
		mFrame += time / mInterval;

		mInterval = mAnimation->get_interval(mFrame);

		mClock.restart();

		update_frame();
		return true;
	}
	return false;
}

bool
animation_node::is_playing() const
{
	return mPlaying;
}

void
animation_node::start()
{
	if (!mPlaying)
		mClock.restart();
	mPlaying = true;
}

void
animation_node::pause()
{
	mClock.pause();
	mPlaying = false;
}

void
animation_node::stop()
{
	mPlaying = false;
	restart();
}

void
animation_node::restart()
{
	if (mAnimation)
	{
		set_frame(mAnimation->get_default_frame());
		mClock.restart();
	}
}

void animation_node::set_color(color pColor)
{
	mSprite.set_color(pColor);
}

void
animation_node::set_anchor(engine::anchor pAnchor)
{
	mAnchor = pAnchor;
	mSprite.set_anchor(pAnchor);
}

void animation_node::set_rotation(float pRotation)
{
	mSprite.set_rotation(pRotation);
}

int
animation_node::draw(renderer &r)
{
	if (!mAnimation) return 1;
	if (mPlaying)
		tick();
	mSprite.draw(r);
	return 0;
}

void animation_node::update_frame()
{
	if (!mAnimation) return;
	frect rect = mAnimation->get_frame_at(mFrame);
	mSprite.set_texture_rect(rect);
	mSprite.set_anchor(mAnchor);
}
