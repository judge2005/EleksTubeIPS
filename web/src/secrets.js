$(document).on("pagecreate", function () {
    console.log("pagecreate");

    $("input[type='password']").each(function() {
        var $input = $(this);
        var $wrapper = $input.parent(".ui-input-text");
        
        // Prevent duplicate buttons if pagecreate fires again
        if ($wrapper.hasClass("has-password-toggle")) {
            return;
        }
        
        $wrapper.addClass("has-password-toggle");
        
        // 1. Track committed values so onchange still fires after type swaps
        $input.data("last-committed-val", $input.val());
        
        $input.on("change", function() {
            $input.data("last-committed-val", $input.val());
        });

        $input.on("blur", function() {
            var currentVal = $input.val();
            if (currentVal !== $input.data("last-committed-val")) {
                $input.trigger("change");
            }
        });
        
        var $btn = $('<a href="#" class="password-toggle-btn ui-btn ui-icon-eye ui-btn-icon-notext ui-corner-all" title="Show password">Show</a>');
        $wrapper.append($btn);
        
        function toggleVisibility(e) {
            e.preventDefault();
            e.stopPropagation();
            
            if ($input.attr("type") === "password") {
                $input.attr("type", "text");
                $btn.addClass("ui-btn-active").attr("title", "Hide password");
            } else {
                $input.attr("type", "password");
                $btn.removeClass("ui-btn-active").attr("title", "Show password");
            }
        }
        
        // Block focus theft on initial touch
        $btn.on("touchstart", function(e) {
            e.preventDefault();
        });
        
        // Execute toggle directly on touchend (mobile) and mousedown (desktop)
        $btn.on("touchend mousedown", function(e) {
            // Ignore synthetic mousedown events fired after touch
            if (e.type === "mousedown" && e.originalEvent && e.originalEvent.sourceCapabilities && e.originalEvent.sourceCapabilities.firesTouchEvents) {
                return;
            }
            toggleVisibility(e);
        });
        
        // Prevent default anchor navigation if click still fires
        $btn.on("click", function(e) {
            e.preventDefault();
        });
    });
});
