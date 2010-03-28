//call this to show a login-dialog and handle everything to let the user login.
function synapse_login()
{
	//create a dialog-div
	html='\
		<div id="synapse_login" title="Please login">\
			<p>Username: <input type="text" class="username">\
			<p>Password: <input type="password" class="password">\
		</div>\
	';
	$('body').append(html);
	$('#synapse_login').dialog({ 
		buttons:
		{
			'Login': function()
			{ 
				send(0,"core_Login", 
				{
					'username': $('#synapse_login .username').val(),
					'password': $('#synapse_login .password').val(),
				});
			}
		},
		'close': function ()
		{ 
			$('#synapse_login').remove(); 
		},
		modal: true,
	});
	$('#synapse_login .username').focus();
};

function synapse_showError(description)
{
	//create a error-dialog?
	if ($('#synapse_error').length==0)
	{
		html='\
			<div id="synapse_error" title="Error">\
				<div class="ui-widget">\
					<div class="ui-state-error ui-corner-all" style="padding: 0 .7em;">\
						<p>\
						<span class="ui-icon ui-icon-alert" style="float: left; margin-right: .3em;"></span>\
						<span class="description"></span>\
						</p>\
					</div>\
				</div>\
			</div>\
		';
		$('body').append(html);
		$('#synapse_error').dialog({ 
			buttons:
			{
				'Ok': function()
				{ 
					$('#synapse_error').remove(); 
				}
			},
			'close': function ()
			{ 
				$('#synapse_error').remove(); 
			},
			'modal': true,
		});
	}
	$('#synapse_error .description').append("<p>"+description+"</p>");
}

synapse_register("module_Error",function(msg_src, msg_dst, msg_event, msg)
{
	synapse_showError(msg["description"]);
});

synapse_register("error",function(description)
{
	synapse_showError(description);
});

synapse_register("module_Login",function(msg_src, msg_dst, msg_event, msg)
{
	$('#synapse_login').remove(); 
});


