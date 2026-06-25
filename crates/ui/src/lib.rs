use gpui::{AppContext, Bounds, Render, Styled, WindowBounds, WindowOptions, div, px, rgb, size};

struct Layout;

impl Render for Layout {
    fn render(&mut self, window: &mut gpui::Window, cx: &mut gpui::prelude::Context<Self>) -> impl gpui::prelude::IntoElement {
        div()
        .bg(rgb(0x303030))
        .size_full()
    }
}

pub fn init() {
    gpui::Application::new().run(move |cx| {
        gpui_component::init(cx);
        let bounds = Bounds::centered(None, size(px(450.), px(350.)), cx);

        let _ = cx.open_window(WindowOptions {
            window_bounds: Some(WindowBounds::Windowed(bounds)),
            ..Default::default()

        }, |_, cx| {
            cx.new(|_| Layout {})
        },);
    });
}