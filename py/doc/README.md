# Publish the documentation

The documentation is published through the `gh-pages` branch.

To publish the documentation, you need to clone the `gh-pages` branch of this repository into
`doc/gh-pages`. In `doc`, do: 

    git clone -b gh-pages git@github.com:larsimmisch/pyalsaaudio.git gh-pages

(This is a bit of a hack)

Once that is set up, you can publish new documentation using:

    make publish

Be careful when new files are generated, however, you will have to add them
manually to git.
