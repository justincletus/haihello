
User module to pull tags from content types to the user profile.

How it works
------------

This module would be a simple method to track users activity on your web site,
using the tag system of drupal.

The module create a taxonomy term field in the user profile and associate to
vocaboulary "tags". Everytime the user visit a node of a content type set in the
module, inherit node tags.

If the default "tags" vocaboulary doesn't exist, the module create a new one
with the same name. If you want to use a different vocaboulary, edit the field
created by the module and associate your vocaboulary.

Installation
------------

Copy user_auto_tag folder to your module directory and then enable
on the admin modules page. You can set your options type at
admin/config/workflow/user_auto_tag.

Author
------
Gianluca Agnocchetti (hiryu)
hiryu@gmx.com
